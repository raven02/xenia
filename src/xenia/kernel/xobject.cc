/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xobject.h"

#include "xenia/base/clock.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/objects/xevent.h"
#include "xenia/kernel/objects/xmutant.h"
#include "xenia/kernel/objects/xsemaphore.h"
#include "xenia/kernel/xboxkrnl_private.h"

namespace xe {
namespace kernel {

XObject::XObject(KernelState* kernel_state, Type type)
    : kernel_state_(kernel_state),
      handle_ref_count_(0),
      pointer_ref_count_(1),
      type_(type),
      handle_(X_INVALID_HANDLE_VALUE),
      guest_object_ptr_(0),
      allocated_guest_object_(false) {
  // Added pointer check to support usage without a kernel_state
  if (kernel_state != nullptr) {
    kernel_state->object_table()->AddHandle(this, &handle_);
  }
}

XObject::~XObject() {
  assert_zero(handle_ref_count_);
  assert_zero(pointer_ref_count_);

  if (allocated_guest_object_) {
    uint32_t ptr = guest_object_ptr_ - sizeof(X_OBJECT_HEADER);
    auto header = memory()->TranslateVirtual<X_OBJECT_HEADER*>(ptr);

    // Free the object creation info
    if (header->object_create_info) {
      memory()->SystemHeapFree(header->object_create_info);
    }

    memory()->SystemHeapFree(ptr);
  }
}

Emulator* XObject::emulator() const { return kernel_state_->emulator_; }
KernelState* XObject::kernel_state() const { return kernel_state_; }
Memory* XObject::memory() const { return kernel_state_->memory(); }

XObject::Type XObject::type() { return type_; }

X_HANDLE XObject::handle() const { return handle_; }

void XObject::RetainHandle() { ++handle_ref_count_; }

bool XObject::ReleaseHandle() {
  if (--handle_ref_count_ == 0) {
    return true;
  }
  return false;
}

void XObject::Retain() { ++pointer_ref_count_; }

void XObject::Release() {
  if (--pointer_ref_count_ == 0) {
    assert_true(pointer_ref_count_ >= handle_ref_count_);
    delete this;
  }
}

X_STATUS XObject::Delete() {
  if (kernel_state_ == nullptr) {
    // Fake return value for api-scanner
    return X_STATUS_SUCCESS;
  } else {
    if (!name_.empty()) {
      kernel_state_->object_table()->RemoveNameMapping(name_);
    }
    return kernel_state_->object_table()->RemoveHandle(handle_);
  }
}

void XObject::SetAttributes(uint32_t obj_attributes_ptr) {
  if (!obj_attributes_ptr) {
    return;
  }

  auto name = X_ANSI_STRING::to_string_indirect(memory()->virtual_membase(),
                                                obj_attributes_ptr + 4);
  if (!name.empty()) {
    name_ = std::move(name);
    kernel_state_->object_table()->AddNameMapping(name_, handle_);
  }
}

uint32_t XObject::TimeoutTicksToMs(int64_t timeout_ticks) {
  if (timeout_ticks > 0) {
    // Absolute time, based on January 1, 1601.
    // TODO(benvanik): convert time to relative time.
    assert_always();
    return 0;
  } else if (timeout_ticks < 0) {
    // Relative time.
    return (uint32_t)(-timeout_ticks / 10000);  // Ticks -> MS
  } else {
    return 0;
  }
}

X_STATUS XObject::Wait(uint32_t wait_reason, uint32_t processor_mode,
                       uint32_t alertable, uint64_t* opt_timeout) {
  void* wait_handle = GetWaitHandle();
  if (!wait_handle) {
    // Object doesn't support waiting.
    return X_STATUS_SUCCESS;
  }

  DWORD timeout_ms = opt_timeout ? TimeoutTicksToMs(*opt_timeout) : INFINITE;
  timeout_ms = Clock::ScaleGuestDurationMillis(timeout_ms);

  DWORD result = WaitForSingleObjectEx(wait_handle, timeout_ms, alertable);
  switch (result) {
    case WAIT_OBJECT_0:
      return X_STATUS_SUCCESS;
    case WAIT_IO_COMPLETION:
      // Or X_STATUS_ALERTED?
      return X_STATUS_USER_APC;
    case WAIT_TIMEOUT:
      YieldProcessor();
      return X_STATUS_TIMEOUT;
    default:
    case WAIT_FAILED:
    case WAIT_ABANDONED:
      return X_STATUS_ABANDONED_WAIT_0;
  }
}

X_STATUS XObject::SignalAndWait(XObject* signal_object, XObject* wait_object,
                                uint32_t wait_reason, uint32_t processor_mode,
                                uint32_t alertable, uint64_t* opt_timeout) {
  DWORD timeout_ms = opt_timeout ? TimeoutTicksToMs(*opt_timeout) : INFINITE;
  timeout_ms = Clock::ScaleGuestDurationMillis(timeout_ms);

  DWORD result = SignalObjectAndWait(signal_object->GetWaitHandle(),
                                     wait_object->GetWaitHandle(), timeout_ms,
                                     alertable ? TRUE : FALSE);

  return result;
}

X_STATUS XObject::WaitMultiple(uint32_t count, XObject** objects,
                               uint32_t wait_type, uint32_t wait_reason,
                               uint32_t processor_mode, uint32_t alertable,
                               uint64_t* opt_timeout) {
  HANDLE* wait_handles = (HANDLE*)alloca(sizeof(HANDLE) * count);
  for (uint32_t n = 0; n < count; n++) {
    wait_handles[n] = objects[n]->GetWaitHandle();
    assert_not_null(wait_handles[n]);
  }

  DWORD timeout_ms = opt_timeout ? TimeoutTicksToMs(*opt_timeout) : INFINITE;
  timeout_ms = Clock::ScaleGuestDurationMillis(timeout_ms);

  DWORD result = WaitForMultipleObjectsEx(
      count, wait_handles, wait_type ? FALSE : TRUE, timeout_ms, alertable);

  return result;
}

uint8_t* XObject::CreateNative(uint32_t size) {
  std::lock_guard<xe::recursive_mutex> lock(kernel_state_->object_mutex());

  uint32_t total_size = size + sizeof(X_OBJECT_HEADER);

  auto mem = memory()->SystemHeapAlloc(total_size);
  if (!mem) {
    // Out of memory!
    return nullptr;
  }

  allocated_guest_object_ = true;
  memory()->Zero(mem, total_size);
  SetNativePointer(mem + sizeof(X_OBJECT_HEADER), true);

  auto header = memory()->TranslateVirtual<X_OBJECT_HEADER*>(mem);

  auto creation_info =
      memory()->SystemHeapAlloc(sizeof(X_OBJECT_CREATE_INFORMATION));
  if (creation_info) {
    memory()->Zero(creation_info, sizeof(X_OBJECT_CREATE_INFORMATION));

    // Set it up in the header.
    // Some kernel method is accessing this struct and dereferencing a member.
    // With our current definition that member is non_paged_pool_charge.
    header->object_create_info = creation_info;
  }

  return memory()->TranslateVirtual(guest_object_ptr_);
}

void XObject::SetNativePointer(uint32_t native_ptr, bool uninitialized) {
  std::lock_guard<xe::recursive_mutex> lock(kernel_state_->object_mutex());

  // If hit: We've already setup the native ptr with CreateNative!
  assert_zero(guest_object_ptr_);

  auto header =
      kernel_state_->memory()->TranslateVirtual<X_DISPATCH_HEADER*>(native_ptr);

  // Memory uninitialized, so don't bother with the check.
  if (!uninitialized) {
    assert_true(!(header->wait_list_blink & 0x1));
  }

  // Stash pointer in struct.
  // FIXME: This assumes the object has a dispatch header (some don't!)
  uint64_t object_ptr = reinterpret_cast<uint64_t>(this);
  object_ptr |= 0x1;
  header->wait_list_flink = (uint32_t)(object_ptr >> 32);
  header->wait_list_blink = (uint32_t)(object_ptr & 0xFFFFFFFF);

  guest_object_ptr_ = native_ptr;
}

object_ref<XObject> XObject::GetNativeObject(KernelState* kernel_state,
                                             void* native_ptr,
                                             int32_t as_type) {
  assert_not_null(native_ptr);

  // Unfortunately the XDK seems to inline some KeInitialize calls, meaning
  // we never see it and just randomly start getting passed events/timers/etc.
  // Luckily it seems like all other calls (Set/Reset/Wait/etc) are used and
  // we don't have to worry about PPC code poking the struct. Because of that,
  // we init on first use, store our pointer in the struct, and dereference it
  // each time.
  // We identify this by checking the low bit of wait_list_blink - if it's 1,
  // we have already put our pointer in there.

  std::lock_guard<xe::recursive_mutex> lock(kernel_state->object_mutex());

  auto header = reinterpret_cast<X_DISPATCH_HEADER*>(native_ptr);

  if (as_type == -1) {
    as_type = header->type;
  }

  if (header->wait_list_blink & 0x1) {
    // Already initialized.
    uint64_t object_ptr = ((uint64_t)header->wait_list_flink << 32) |
                          ((header->wait_list_blink) & ~0x1);
    XObject* object = reinterpret_cast<XObject*>(object_ptr);
    // TODO(benvanik): assert nothing has been changed in the struct.
    return retain_object<XObject>(object);
  } else {
    // First use, create new.
    // http://www.nirsoft.net/kernel_struct/vista/KOBJECTS.html
    XObject* object = nullptr;
    switch (as_type) {
      case 0:  // EventNotificationObject
      case 1:  // EventSynchronizationObject
      {
        auto ev = new XEvent(kernel_state);
        ev->InitializeNative(native_ptr, *header);
        object = ev;
      } break;
      case 2:  // MutantObject
      {
        auto mutant = new XMutant(kernel_state);
        mutant->InitializeNative(native_ptr, *header);
        object = mutant;
      } break;
      case 5:  // SemaphoreObject
      {
        auto sem = new XSemaphore(kernel_state);
        sem->InitializeNative(native_ptr, *header);
        object = sem;
      } break;
      case 3:   // ProcessObject
      case 4:   // QueueObject
      case 6:   // ThreadObject
      case 7:   // GateObject
      case 8:   // TimerNotificationObject
      case 9:   // TimerSynchronizationObject
      case 18:  // ApcObject
      case 19:  // DpcObject
      case 20:  // DeviceQueueObject
      case 21:  // EventPairObject
      case 22:  // InterruptObject
      case 23:  // ProfileObject
      case 24:  // ThreadedDpcObject
      default:
        assert_always();
        return NULL;
    }

    // Stash pointer in struct.
    // FIXME: This assumes the object contains a dispatch header (some don't!)
    uint64_t object_ptr = reinterpret_cast<uint64_t>(object);
    object_ptr |= 0x1;
    header->wait_list_flink = (uint32_t)(object_ptr >> 32);
    header->wait_list_blink = (uint32_t)(object_ptr & 0xFFFFFFFF);

    // NOTE: we are double-retaining, as the object is implicitly created and
    // can never be released.
    return retain_object<XObject>(object);
  }
}

}  // namespace kernel
}  // namespace xe
