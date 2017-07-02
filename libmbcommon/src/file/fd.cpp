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

#include "mbcommon/file/fd.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mbcommon/locale.h"

#include "mbcommon/file/callbacks.h"
#include "mbcommon/file/fd_p.h"

#define DEFAULT_MODE \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

/*!
 * \file mbcommon/file/fd.h
 * \brief Open file with POSIX file descriptors API
 */

namespace mb
{

struct RealFdFileFuncs : public FdFileFuncs
{
#ifdef _WIN32
    virtual int fn_wopen(const wchar_t *path, int flags, mode_t mode) override
    {
        return _wopen(path, flags, mode);
    }
#else
    virtual int fn_open(const char *path, int flags, mode_t mode) override
    {
        return open(path, flags, mode);
    }
#endif

    virtual int fn_fstat(int fildes, struct stat *buf) override
    {
        return fstat(fildes, buf);
    }

    virtual int fn_close(int fd) override
    {
        return close(fd);
    }

    virtual int fn_ftruncate64(int fd, off_t length) override
    {
        return ftruncate64(fd, length);
    }

    virtual off64_t fn_lseek64(int fd, off64_t offset, int whence) override
    {
        return lseek64(fd, offset, whence);
    }

    virtual ssize_t fn_read(int fd, void *buf, size_t count) override
    {
        return read(fd, buf, count);
    }

    virtual ssize_t fn_write(int fd, const void *buf, size_t count) override
    {
        return write(fd, buf, count);
    }
};

static RealFdFileFuncs g_default_funcs;

static void free_ctx(FdFileCtx *ctx)
{
    delete ctx;
}

static FileStatus fd_open_cb(File &file, void *userdata)
{
    FdFileCtx *ctx = static_cast<FdFileCtx *>(userdata);
    struct stat sb;

    if (!ctx->filename.empty()) {
#ifdef _WIN32
        ctx->fd = ctx->funcs->fn_wopen(
#else
        ctx->fd = ctx->funcs->fn_open(
#endif
                ctx->filename.c_str(), ctx->flags, DEFAULT_MODE);
        if (ctx->fd < 0) {
            file.set_error(-errno, "Failed to open file: %s", strerror(errno));
            return FileStatus::FAILED;
        }
    }

    if (ctx->funcs->fn_fstat(ctx->fd, &sb) < 0) {
        file.set_error(-errno, "Failed to stat file: %s", strerror(errno));
        return FileStatus::FAILED;
    }

    if (S_ISDIR(sb.st_mode)) {
        file.set_error(-EISDIR, "Cannot open directory");
        return FileStatus::FAILED;
    }

    return FileStatus::OK;
}

static FileStatus fd_close_cb(File &file, void *userdata)
{
    FdFileCtx *ctx = static_cast<FdFileCtx *>(userdata);
    FileStatus ret = FileStatus::OK;

    if (ctx->owned && ctx->fd >= 0 && ctx->funcs->fn_close(ctx->fd) < 0) {
        file.set_error(-errno, "Failed to close file: %s", strerror(errno));
        ret = FileStatus::FAILED;
    }

    free_ctx(ctx);

    return ret;
}

static FileStatus fd_read_cb(File &file, void *userdata,
                             void *buf, size_t size, size_t *bytes_read)
{
    FdFileCtx *ctx = static_cast<FdFileCtx *>(userdata);

    if (size > SSIZE_MAX) {
        size = SSIZE_MAX;
    }

    ssize_t n = ctx->funcs->fn_read(ctx->fd, buf, size);
    if (n < 0) {
        file.set_error(-errno, "Failed to read file: %s", strerror(errno));
        return errno == EINTR ? FileStatus::RETRY : FileStatus::FAILED;
    }

    *bytes_read = n;
    return FileStatus::OK;
}

static FileStatus fd_write_cb(File &file, void *userdata,
                              const void *buf, size_t size,
                              size_t *bytes_written)
{
    FdFileCtx *ctx = static_cast<FdFileCtx *>(userdata);

    if (size > SSIZE_MAX) {
        size = SSIZE_MAX;
    }

    ssize_t n = ctx->funcs->fn_write(ctx->fd, buf, size);
    if (n < 0) {
        file.set_error(-errno, "Failed to write file: %s", strerror(errno));
        return errno == EINTR ? FileStatus::RETRY : FileStatus::FAILED;
    }

    *bytes_written = n;
    return FileStatus::OK;
}

static FileStatus fd_seek_cb(File &file, void *userdata,
                             int64_t offset, int whence, uint64_t *new_offset)
{
    FdFileCtx *ctx = static_cast<FdFileCtx *>(userdata);

    off64_t ret = ctx->funcs->fn_lseek64(ctx->fd, offset, whence);
    if (ret < 0) {
        file.set_error(-errno, "Failed to seek file: %s", strerror(errno));
        return FileStatus::FAILED;
    }

    *new_offset = ret;
    return FileStatus::OK;
}

static FileStatus fd_truncate_cb(File &file, void *userdata, uint64_t size)
{
    FdFileCtx *ctx = static_cast<FdFileCtx *>(userdata);

    if (ctx->funcs->fn_ftruncate64(ctx->fd, size) < 0) {
        file.set_error(-errno,
                       "Failed to truncate file: %s", strerror(errno));
        return FileStatus::FAILED;
    }

    return FileStatus::OK;
}

static FdFileCtx * create_ctx(File &file, FdFileFuncs *funcs)
{
    FdFileCtx *ctx = new(std::nothrow) FdFileCtx();
    if (!ctx) {
        file.set_error(FileError::INTERNAL_ERROR,
                       "Failed to allocate FdFileCtx: %s",
                       strerror(errno));
        return nullptr;
    }

    ctx->funcs = funcs;

    return ctx;
}

static FileStatus open_ctx(File &file, FdFileCtx *ctx)
{
    return file_open_callbacks(file,
                               &fd_open_cb,
                               &fd_close_cb,
                               &fd_read_cb,
                               &fd_write_cb,
                               &fd_seek_cb,
                               &fd_truncate_cb,
                               ctx);
}

static int convert_mode(FileOpenMode mode)
{
    int ret = 0;

#ifdef _WIN32
    ret |= O_NOINHERIT | O_BINARY;
#else
    ret |= O_CLOEXEC;
#endif

    switch (mode) {
    case FileOpenMode::READ_ONLY:
        ret |= O_RDONLY;
        break;
    case FileOpenMode::READ_WRITE:
        ret |= O_RDWR;
        break;
    case FileOpenMode::WRITE_ONLY:
        ret |= O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case FileOpenMode::READ_WRITE_TRUNC:
        ret |= O_RDWR | O_CREAT | O_TRUNC;
        break;
    case FileOpenMode::APPEND:
        ret |= O_WRONLY | O_CREAT | O_APPEND;
        break;
    case FileOpenMode::READ_APPEND:
        ret |= O_RDWR | O_CREAT | O_APPEND;
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

FileStatus _file_open_fd(FdFileFuncs *funcs, File &file, int fd, bool owned)
{
    FdFileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->fd = fd;
    ctx->owned = owned;

    return open_ctx(file, ctx);
}

FileStatus _file_open_fd_filename(FdFileFuncs *funcs, File &file,
                                  const char *filename, FileOpenMode mode)
{
    FdFileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->owned = true;

#ifdef _WIN32
    if (!mb::mbs_to_wcs(ctx->filename, filename)) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Failed to convert MBS filename or mode to WCS");
        free_ctx(ctx);
        return FileStatus::FATAL;
    }
#else
    ctx->filename = filename;
#endif

    ctx->flags = convert_mode(mode);
    if (ctx->flags < 0) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Invalid mode: %d", mode);
        free_ctx(ctx);
        return FileStatus::FATAL;
    }

    return open_ctx(file, ctx);
}

FileStatus _file_open_fd_filename_w(FdFileFuncs *funcs, File &file,
                                    const wchar_t *filename, FileOpenMode mode)
{
    FdFileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->owned = true;

#ifdef _WIN32
    ctx->filename = filename;
#else
    if (!mb::wcs_to_mbs(ctx->filename, filename)) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Failed to convert WCS filename or mode to MBS");
        free_ctx(ctx);
        return FileStatus::FATAL;
    }
#endif

    ctx->flags = convert_mode(mode);
    if (ctx->flags < 0) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Invalid mode: %d", mode);
        free_ctx(ctx);
        return FileStatus::FATAL;
    }

    return open_ctx(file, ctx);
}

/*!
 * Open File handle from file descriptor.
 *
 * If \p owned is true, then the File handle will take ownership of the file
 * descriptor. In other words, the file descriptor will be closed when the
 * File handle is closed.
 *
 * \param file File handle
 * \param fd File descriptor
 * \param owned Whether the file descriptor should be owned by the File handle
 *
 * \return
 *   * #FileStatus::OK if the file descriptor was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_fd(File &file, int fd, bool owned)
{
    return _file_open_fd(&g_default_funcs, file, fd, owned);
}

/*!
 * Open File handle from a multi-byte filename.
 *
 * On Unix-like systems, \p filename is directly passed to `open()`. On Windows
 * systems, \p filename is converted to WCS using mb::mbs_to_wcs() before being
 * passed to `_wopen()`.
 *
 * \param file File handle
 * \param filename MBS filename
 * \param mode Open mode (\ref FileOpenMode)
 *
 * \return
 *   * #FileStatus::OK if the file was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_fd_filename(File &file, const char *filename,
                                 FileOpenMode mode)
{
    return _file_open_fd_filename(&g_default_funcs, file, filename, mode);
}

/*!
 * Open File handle from a wide-character filename.
 *
 * On Unix-like systems, \p filename is converted to MBS using mb::wcs_to_mbs()
 * before being passed to `open()`. On Windows systems, \p filename is directly
 * passed to `_wopen()`.
 *
 * \param file File handle
 * \param filename WCS filename
 * \param mode Open mode (\ref FileOpenMode)
 *
 * \return
 *   * #FileStatus::OK if the file was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_fd_filename_w(File &file, const wchar_t *filename,
                                   FileOpenMode mode)
{
    return _file_open_fd_filename_w(&g_default_funcs, file, filename, mode);
}

}
