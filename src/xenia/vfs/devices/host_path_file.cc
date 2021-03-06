/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/host_path_file.h"

#include "xenia/vfs/devices/host_path_entry.h"

namespace xe {
namespace vfs {

HostPathFile::HostPathFile(KernelState* kernel_state, uint32_t file_access,
                           HostPathEntry* entry, HANDLE file_handle)
    : XFile(kernel_state, file_access, entry),
      entry_(entry),
      file_handle_(file_handle) {}

HostPathFile::~HostPathFile() { CloseHandle(file_handle_); }

X_STATUS HostPathFile::ReadSync(void* buffer, size_t buffer_length,
                                size_t byte_offset, size_t* out_bytes_read) {
  if (!(file_access() & FileAccess::kFileReadData)) {
    return X_STATUS_ACCESS_DENIED;
  }

  OVERLAPPED overlapped;
  overlapped.Pointer = (PVOID)byte_offset;
  overlapped.hEvent = NULL;
  DWORD bytes_read = 0;
  BOOL read = ReadFile(file_handle_, buffer, (DWORD)buffer_length, &bytes_read,
                       &overlapped);
  if (read) {
    *out_bytes_read = bytes_read;
    return X_STATUS_SUCCESS;
  } else {
    return X_STATUS_END_OF_FILE;
  }
}

X_STATUS HostPathFile::WriteSync(const void* buffer, size_t buffer_length,
                                 size_t byte_offset,
                                 size_t* out_bytes_written) {
  if (!(file_access() & FileAccess::kFileWriteData |
        FileAccess::kFileAppendData)) {
    return X_STATUS_ACCESS_DENIED;
  }

  OVERLAPPED overlapped;
  overlapped.Pointer = (PVOID)byte_offset;
  overlapped.hEvent = NULL;
  DWORD bytes_written = 0;
  BOOL wrote = WriteFile(file_handle_, buffer, (DWORD)buffer_length,
                         &bytes_written, &overlapped);
  if (wrote) {
    *out_bytes_written = bytes_written;
    return X_STATUS_SUCCESS;
  } else {
    return X_STATUS_END_OF_FILE;
  }
}

}  // namespace vfs
}  // namespace xe
