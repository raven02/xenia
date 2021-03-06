/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/host_path_entry.h"

#include "xenia/base/logging.h"
#include "xenia/base/mapped_memory.h"
#include "xenia/base/math.h"
#include "xenia/base/string.h"
#include "xenia/vfs/devices/host_path_file.h"

namespace xe {
namespace vfs {

HostPathEntry::HostPathEntry(Device* device, Entry* parent, std::string path,
                             const std::wstring& local_path)
    : Entry(device, parent, path), local_path_(local_path) {}

HostPathEntry::~HostPathEntry() = default;

HostPathEntry* HostPathEntry::Create(Device* device, Entry* parent,
                                     const std::wstring& full_path,
                                     xe::filesystem::FileInfo file_info) {
  auto entry = new HostPathEntry(device, parent, xe::to_string(file_info.name),
                                 full_path);
  entry->create_timestamp_ = file_info.create_timestamp;
  entry->access_timestamp_ = file_info.access_timestamp;
  entry->write_timestamp_ = file_info.write_timestamp;
  if (file_info.type == xe::filesystem::FileInfo::Type::kDirectory) {
    entry->attributes_ = kFileAttributeDirectory;
  } else {
    entry->attributes_ = kFileAttributeNormal;
    if (device->is_read_only()) {
      entry->attributes_ |= kFileAttributeReadOnly;
    }
    entry->size_ = file_info.total_size;
    entry->allocation_size_ =
        xe::round_up(file_info.total_size, device->bytes_per_sector());
  }
  return entry;
}

X_STATUS HostPathEntry::Open(KernelState* kernel_state, uint32_t desired_access,
                             object_ref<XFile>* out_file) {
  if (is_read_only() && (desired_access & (FileAccess::kFileWriteData |
                                           FileAccess::kFileAppendData))) {
    XELOGE("Attempting to open file for write access on read-only device");
    return X_STATUS_ACCESS_DENIED;
  }
  DWORD open_access = 0;
  if (desired_access & FileAccess::kGenericRead) {
    open_access |= GENERIC_READ;
  }
  if (desired_access & FileAccess::kGenericWrite) {
    open_access |= GENERIC_WRITE;
  }
  if (desired_access & FileAccess::kGenericExecute) {
    open_access |= GENERIC_EXECUTE;
  }
  if (desired_access & FileAccess::kGenericAll) {
    open_access |= GENERIC_ALL;
  }
  if (desired_access & FileAccess::kFileReadData) {
    open_access |= FILE_READ_DATA;
  }
  if (desired_access & FileAccess::kFileWriteData) {
    open_access |= FILE_WRITE_DATA;
  }
  if (desired_access & FileAccess::kFileAppendData) {
    open_access |= FILE_APPEND_DATA;
  }
  DWORD share_mode = FILE_SHARE_READ;
  // We assume we've already created the file in the caller.
  DWORD creation_disposition = OPEN_EXISTING;
  HANDLE file =
      CreateFileW(local_path_.c_str(), desired_access, share_mode, NULL,
                  creation_disposition, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    // TODO(benvanik): pick correct response.
    return X_STATUS_NO_SUCH_FILE;
  }

  *out_file = object_ref<XFile>(
      new HostPathFile(kernel_state, desired_access, this, file));
  return X_STATUS_SUCCESS;
}

std::unique_ptr<MappedMemory> HostPathEntry::OpenMapped(MappedMemory::Mode mode,
                                                        size_t offset,
                                                        size_t length) {
  return MappedMemory::Open(local_path_, mode, offset, length);
}

std::unique_ptr<Entry> HostPathEntry::CreateEntryInternal(std::string name,
                                                          uint32_t attributes) {
  auto full_path = xe::join_paths(local_path_, xe::to_wstring(name));
  if (attributes & kFileAttributeDirectory) {
    if (!xe::filesystem::CreateFolder(full_path)) {
      return nullptr;
    }
  } else {
    auto file = xe::filesystem::OpenFile(full_path, "wb");
    if (!file) {
      return nullptr;
    }
    fclose(file);
  }
  xe::filesystem::FileInfo file_info;
  if (!xe::filesystem::GetInfo(full_path, &file_info)) {
    return nullptr;
  }
  return std::unique_ptr<Entry>(
      HostPathEntry::Create(device_, this, full_path, file_info));
}

bool HostPathEntry::DeleteEntryInternal(Entry* entry) {
  auto full_path = xe::join_paths(local_path_, xe::to_wstring(entry->name()));
  if (entry->attributes() & kFileAttributeDirectory) {
    // Delete entire directory and contents.
    return xe::filesystem::DeleteFolder(full_path);
  } else {
    // Delete file.
    return xe::filesystem::DeleteFile(full_path);
  }
}

}  // namespace vfs
}  // namespace xe
