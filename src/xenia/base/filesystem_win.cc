/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2015 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/filesystem.h"

#include <string>

#include "xenia/base/platform.h"

namespace xe {
namespace filesystem {

bool PathExists(const std::wstring& path) {
  DWORD attrib = GetFileAttributes(path.c_str());
  return attrib != INVALID_FILE_ATTRIBUTES;
}

bool CreateFolder(const std::wstring& path) {
  wchar_t folder[MAX_PATH] = {0};
  auto end = std::wcschr(path.c_str(), xe::wpath_separator);
  while (end) {
    wcsncpy(folder, path.c_str(), end - path.c_str() + 1);
    CreateDirectory(folder, NULL);
    end = wcschr(++end, xe::wpath_separator);
  }
  return PathExists(path);
}

bool DeleteFolder(const std::wstring& path) {
  auto double_null_path = path + L"\0";
  SHFILEOPSTRUCT op = {0};
  op.wFunc = FO_DELETE;
  op.pFrom = double_null_path.c_str();
  op.fFlags = FOF_NO_UI;
  return SHFileOperation(&op) == 0;
}

bool IsFolder(const std::wstring& path) {
  DWORD attrib = GetFileAttributes(path.c_str());
  return attrib != INVALID_FILE_ATTRIBUTES &&
         (attrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
}

FILE* OpenFile(const std::wstring& path, const char* mode) {
  auto fixed_path = xe::fix_path_separators(path);
  return _wfopen(fixed_path.c_str(), xe::to_wstring(mode).c_str());
}

bool DeleteFile(const std::wstring& path) {
  return DeleteFileW(path.c_str()) ? true : false;
}

#define COMBINE_TIME(t) (((uint64_t)t.dwHighDateTime << 32) | t.dwLowDateTime)

bool GetInfo(const std::wstring& path, FileInfo* out_info) {
  std::memset(out_info, 0, sizeof(FileInfo));
  WIN32_FILE_ATTRIBUTE_DATA data = {0};
  if (!GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &data)) {
    return false;
  }
  if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    out_info->type = FileInfo::Type::kDirectory;
    out_info->total_size = 0;
  } else {
    out_info->type = FileInfo::Type::kFile;
    out_info->total_size =
        (data.nFileSizeHigh * (size_t(MAXDWORD) + 1)) + data.nFileSizeLow;
  }
  out_info->name = xe::find_name_from_path(path);
  out_info->create_timestamp = COMBINE_TIME(data.ftCreationTime);
  out_info->access_timestamp = COMBINE_TIME(data.ftLastAccessTime);
  out_info->write_timestamp = COMBINE_TIME(data.ftLastWriteTime);
  return true;
}

std::vector<FileInfo> ListFiles(const std::wstring& path) {
  std::vector<FileInfo> result;

  WIN32_FIND_DATA ffd;
  HANDLE handle = FindFirstFile((path + L"\\*").c_str(), &ffd);
  if (handle == INVALID_HANDLE_VALUE) {
    return result;
  }
  do {
    if (std::wcscmp(ffd.cFileName, L".") == 0 ||
        std::wcscmp(ffd.cFileName, L"..") == 0) {
      continue;
    }
    FileInfo info;
    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      info.type = FileInfo::Type::kDirectory;
      info.total_size = 0;
    } else {
      info.type = FileInfo::Type::kFile;
      info.total_size =
          (ffd.nFileSizeHigh * (size_t(MAXDWORD) + 1)) + ffd.nFileSizeLow;
    }
    info.name = ffd.cFileName;
    info.create_timestamp = COMBINE_TIME(ffd.ftCreationTime);
    info.access_timestamp = COMBINE_TIME(ffd.ftLastAccessTime);
    info.write_timestamp = COMBINE_TIME(ffd.ftLastWriteTime);
    result.push_back(info);
  } while (FindNextFile(handle, &ffd) != 0);
  FindClose(handle);

  return result;
}

}  // namespace filesystem
}  // namespace xe
