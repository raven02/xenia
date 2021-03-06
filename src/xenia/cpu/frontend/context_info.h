/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_FRONTEND_CONTEXT_INFO_H_
#define XENIA_FRONTEND_CONTEXT_INFO_H_

#include <cstddef>
#include <cstdint>

namespace xe {
namespace cpu {
namespace frontend {

class ContextInfo {
 public:
  ContextInfo(size_t size, uintptr_t thread_state_offset,
              uintptr_t thread_id_offset);
  ~ContextInfo();

  size_t size() const { return size_; }

  uintptr_t thread_state_offset() const { return thread_state_offset_; }
  uintptr_t thread_id_offset() const { return thread_id_offset_; }

 private:
  size_t size_;
  uintptr_t thread_state_offset_;
  uintptr_t thread_id_offset_;
};

}  // namespace frontend
}  // namespace cpu
}  // namespace xe

#endif  // XENIA_FRONTEND_CONTEXT_INFO_H_
