/*
 * Copyright (C) 2016-2017  Andrew Gunnerson <andrewgunnerson@gmail.com>
 *
 * This file is part of DualBootPatcher
 *
 * DualBootPatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DualBootPatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DualBootPatcher.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "mbcommon/common.h"

#include <memory>
#include <string>

#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace mb
{

enum class FileStatus
{
    OK              = 0,
    RETRY           = -1,
    UNSUPPORTED     = -2,
    WARN            = -3,
    FAILED          = -4,
    FATAL           = -5,
};

// Cannot be an enum class because these can be returned along with
// errno/GetLastError() in File::error().
namespace FileError
{

constexpr int NONE              = 0;
constexpr int INVALID_ARGUMENT  = 1;
constexpr int UNSUPPORTED       = 2;
constexpr int PROGRAMMER_ERROR  = 3;
constexpr int INTERNAL_ERROR    = 4;

}

class FilePrivate;
class MB_EXPORT File
{
    MB_DECLARE_PRIVATE(File)

public:
    typedef FileStatus (*OpenCb)(File &file, void *userdata);
    typedef FileStatus (*CloseCb)(File &file, void *userdata);
    typedef FileStatus (*ReadCb)(File &file, void *userdata,
                                 void *buf, size_t size,
                                 size_t *bytes_read);
    typedef FileStatus (*WriteCb)(File &file, void *userdata,
                                  const void *buf, size_t size,
                                  size_t *bytes_written);
    typedef FileStatus (*SeekCb)(File &file, void *userdata,
                                 int64_t offset, int whence,
                                 uint64_t *new_offset);
    typedef FileStatus (*TruncateCb)(File &file, void *userdata,
                                     uint64_t size);

    File();
    virtual ~File();

    // Callbacks
    FileStatus set_open_callback(OpenCb open_cb);
    FileStatus set_close_callback(CloseCb close_cb);
    FileStatus set_read_callback(ReadCb read_cb);
    FileStatus set_write_callback(WriteCb write_cb);
    FileStatus set_seek_callback(SeekCb seek_cb);
    FileStatus set_truncate_callback(TruncateCb truncate_cb);
    FileStatus set_callback_data(void *userdata);

    // File open/close
    FileStatus open();
    FileStatus close();

    // File operations
    FileStatus read(void *buf, size_t size, size_t *bytes_read);
    FileStatus write(const void *buf, size_t size, size_t *bytes_written);
    FileStatus seek(int64_t offset, int whence, uint64_t *new_offset);
    FileStatus truncate(uint64_t size);

    // Error handling functions
    int error();
    std::string error_string();
    MB_PRINTF(3, 4)
    FileStatus set_error(int error_code, const char *fmt, ...);
    FileStatus set_error_v(int error_code, const char *fmt, va_list ap);

    File(const File &) = delete;
    File(File &&) = default;
    File & operator=(const File &) & = delete;
    File & operator=(File &&) & = default;

protected:
    File(FilePrivate *priv);

    std::unique_ptr<FilePrivate> _priv_ptr;
};

}
