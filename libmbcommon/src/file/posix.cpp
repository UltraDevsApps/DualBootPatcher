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

#include "mbcommon/file/posix.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "mbcommon/locale.h"

#include "mbcommon/file/callbacks.h"
#include "mbcommon/file/posix_p.h"

#ifndef __ANDROID__
static_assert(sizeof(off_t) > 4, "Not compiling with LFS support!");
#endif

/*!
 * \file mbcommon/file/posix.h
 * \brief Open file with POSIX stdio `FILE *` API
 */

namespace mb
{

struct RealPosixFileFuncs : public PosixFileFuncs
{
    virtual int fn_fstat(int fildes, struct stat *buf) override
    {
        return fstat(fildes, buf);
    }

    virtual int fn_fclose(FILE *stream) override
    {
        return fclose(stream);
    }

    virtual int fn_ferror(FILE *stream) override
    {
        return ferror(stream);
    }

    virtual int fn_fileno(FILE *stream) override
    {
        return fileno(stream);
    }

#ifdef _WIN32
    virtual FILE * fn_wfopen(const wchar_t *filename,
                             const wchar_t *mode) override
    {
        return _wfopen(filename, mode);
    }
#else
    virtual FILE * fn_fopen(const char *path, const char *mode) override
    {
        return fopen(path, mode);
    }
#endif

    virtual size_t fn_fread(void *ptr, size_t size, size_t nmemb,
                            FILE *stream) override
    {
        return fread(ptr, size, nmemb, stream);
    }

    virtual int fn_fseeko(FILE *stream, off_t offset, int whence) override
    {
        return fseeko(stream, offset, whence);
    }

    virtual off_t fn_ftello(FILE *stream) override
    {
        return ftello(stream);
    }

    virtual size_t fn_fwrite(const void *ptr, size_t size, size_t nmemb,
                             FILE *stream) override
    {
        return fwrite(ptr, size, nmemb, stream);
    }

    virtual int fn_ftruncate64(int fd, off_t length) override
    {
        return ftruncate64(fd, length);
    }
};

static RealPosixFileFuncs g_default_funcs;

static void free_ctx(PosixFileCtx *ctx)
{
    delete ctx;
}

static FileStatus posix_open_cb(File &file, void *userdata)
{
    PosixFileCtx *ctx = static_cast<PosixFileCtx *>(userdata);
    struct stat sb;
    int fd;

    if (!ctx->filename.empty()) {
#ifdef _WIN32
        ctx->fp = ctx->funcs->fn_wfopen(
#else
        ctx->fp = ctx->funcs->fn_fopen(
#endif
                ctx->filename.c_str(), ctx->mode);
        if (!ctx->fp) {
            file.set_error(-errno, "Failed to open file: %s", strerror(errno));
            return FileStatus::FAILED;
        }
    }

    fd = ctx->funcs->fn_fileno(ctx->fp);
    if (fd >= 0) {
        if (ctx->funcs->fn_fstat(fd, &sb) < 0) {
            file.set_error(-errno, "Failed to stat file: %s", strerror(errno));
            return FileStatus::FAILED;
        }

        if (S_ISDIR(sb.st_mode)) {
            file.set_error(-EISDIR, "Cannot open directory");
            return FileStatus::FAILED;
        }

        // Enable seekability based on file type because lseek(fd, 0, SEEK_CUR)
        // does not always fail on non-seekable file descriptors
        if (S_ISREG(sb.st_mode)
#ifdef __linux__
                || S_ISBLK(sb.st_mode)
#endif
        ) {
            ctx->can_seek = true;
        }
    }

    return FileStatus::OK;
}

static FileStatus posix_close_cb(File &file, void *userdata)
{
    PosixFileCtx *ctx = static_cast<PosixFileCtx *>(userdata);
    FileStatus ret = FileStatus::OK;

    if (ctx->owned && ctx->fp && ctx->funcs->fn_fclose(ctx->fp) == EOF) {
        file.set_error(-errno, "Failed to close file: %s", strerror(errno));
        ret = FileStatus::FAILED;
    }

    // Clean up resources
    free_ctx(ctx);

    return ret;
}

static FileStatus posix_read_cb(File &file, void *userdata,
                                void *buf, size_t size, size_t *bytes_read)
{
    PosixFileCtx *ctx = static_cast<PosixFileCtx *>(userdata);

    size_t n = ctx->funcs->fn_fread(buf, 1, size, ctx->fp);

    if (n < size && ctx->funcs->fn_ferror(ctx->fp)) {
        file.set_error(-errno, "Failed to read file: %s", strerror(errno));
        return errno == EINTR ? FileStatus::RETRY : FileStatus::FAILED;
    }

    *bytes_read = n;
    return FileStatus::OK;
}

static FileStatus posix_write_cb(File &file, void *userdata,
                                 const void *buf, size_t size,
                                 size_t *bytes_written)
{
    PosixFileCtx *ctx = static_cast<PosixFileCtx *>(userdata);

    size_t n = ctx->funcs->fn_fwrite(buf, 1, size, ctx->fp);

    if (n < size && ctx->funcs->fn_ferror(ctx->fp)) {
        file.set_error(-errno, "Failed to write file: %s", strerror(errno));
        return errno == EINTR ? FileStatus::RETRY : FileStatus::FAILED;
    }

    *bytes_written = n;
    return FileStatus::OK;
}

static FileStatus posix_seek_cb(File &file, void *userdata,
                                int64_t offset, int whence,
                                uint64_t *new_offset)
{
    PosixFileCtx *ctx = static_cast<PosixFileCtx *>(userdata);
    off64_t old_pos, new_pos;

    if (!ctx->can_seek) {
        file.set_error(FileError::UNSUPPORTED,
                       "Seek not supported: %s", strerror(errno));
        return FileStatus::UNSUPPORTED;
    }

    // Get current file position
    old_pos = ctx->funcs->fn_ftello(ctx->fp);
    if (old_pos < 0) {
        file.set_error(-errno, "Failed to get file position: %s",
                       strerror(errno));
        return FileStatus::FAILED;
    }

    // Try to seek
    if (ctx->funcs->fn_fseeko(ctx->fp, offset, whence) < 0) {
        file.set_error(-errno, "Failed to seek file: %s", strerror(errno));
        return FileStatus::FAILED;
    }

    // Get new position
    new_pos = ctx->funcs->fn_ftello(ctx->fp);
    if (new_pos < 0) {
        // Try to restore old position
        file.set_error(-errno, "Failed to get file position: %s",
                       strerror(errno));
        return ctx->funcs->fn_fseeko(ctx->fp, old_pos, SEEK_SET) == 0
                ? FileStatus::FAILED : FileStatus::FATAL;
    }

    *new_offset = new_pos;
    return FileStatus::OK;
}

static FileStatus posix_truncate_cb(File &file, void *userdata, uint64_t size)
{
    PosixFileCtx *ctx = static_cast<PosixFileCtx *>(userdata);

    int fd = ctx->funcs->fn_fileno(ctx->fp);
    if (fd < 0) {
        file.set_error(FileError::UNSUPPORTED,
                       "fileno() not supported for fp");
        return FileStatus::UNSUPPORTED;
    }

    if (ctx->funcs->fn_ftruncate64(fd, size) < 0) {
        file.set_error(-errno, "Failed to truncate file: %s", strerror(errno));
        return FileStatus::FAILED;
    }

    return FileStatus::OK;
}

static PosixFileCtx * create_ctx(File &file, PosixFileFuncs *funcs)
{
    PosixFileCtx *ctx = new(std::nothrow) PosixFileCtx();
    if (!ctx) {
        file.set_error(FileError::INTERNAL_ERROR,
                       "Failed to allocate PosixFileCtx: %s",
                       strerror(errno));
        return nullptr;
    }

    ctx->funcs = funcs;

    return ctx;
}

static FileStatus open_ctx(File &file, PosixFileCtx *ctx)
{
    return file_open_callbacks(file,
                               &posix_open_cb,
                               &posix_close_cb,
                               &posix_read_cb,
                               &posix_write_cb,
                               &posix_seek_cb,
                               &posix_truncate_cb,
                               ctx);
}

#ifdef _WIN32
static const wchar_t * convert_mode(FileOpenMode mode)
{
    switch (mode) {
    case FileOpenMode::READ_ONLY:
        return L"rbN";
    case FileOpenMode::READ_WRITE:
        return L"r+bN";
    case FileOpenMode::WRITE_ONLY:
        return L"wbN";
    case FileOpenMode::READ_WRITE_TRUNC:
        return L"w+bN";
    case FileOpenMode::APPEND:
        return L"abN";
    case FileOpenMode::READ_APPEND:
        return L"a+bN";
    default:
        return nullptr;
    }
}
#else
static const char * convert_mode(FileOpenMode mode)
{
    switch (mode) {
    case FileOpenMode::READ_ONLY:
        return "rbe";
    case FileOpenMode::READ_WRITE:
        return "r+be";
    case FileOpenMode::WRITE_ONLY:
        return "wbe";
    case FileOpenMode::READ_WRITE_TRUNC:
        return "w+be";
    case FileOpenMode::APPEND:
        return "abe";
    case FileOpenMode::READ_APPEND:
        return "a+be";
    default:
        return nullptr;
    }
}
#endif

FileStatus _file_open_FILE(PosixFileFuncs *funcs, File &file, FILE *fp,
                           bool owned)
{
    PosixFileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->fp = fp;
    ctx->owned = owned;

    return open_ctx(file, ctx);
}

FileStatus _file_open_FILE_filename(PosixFileFuncs *funcs, File &file,
                                    const char *filename,
                                    FileOpenMode mode)
{
    PosixFileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->owned = true;

#ifdef _WIN32
    if (!mb::mbs_to_wcs(ctx->filename, filename)) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Failed to convert MBS filename to WCS");
        free_ctx(ctx);
        return FileStatus::FATAL;
    }
#else
    ctx->filename = filename;
#endif

    ctx->mode = convert_mode(mode);
    if (!ctx->mode) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Invalid mode: %d", mode);
        free_ctx(ctx);
        return FileStatus::FATAL;
    }

    return open_ctx(file, ctx);
}

FileStatus _file_open_FILE_filename_w(PosixFileFuncs *funcs, File &file,
                                      const wchar_t *filename,
                                      FileOpenMode mode)
{
    PosixFileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->owned = true;

#ifdef _WIN32
    ctx->filename = filename;
#else
    if (!mb::wcs_to_mbs(ctx->filename, filename)) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Failed to convert WCS filename to MBS");
        free_ctx(ctx);
        return FileStatus::FATAL;
    }
#endif

    ctx->mode = convert_mode(mode);
    if (!ctx->mode) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Invalid mode: %d", mode);
        free_ctx(ctx);
        return FileStatus::FATAL;
    }

    return open_ctx(file, ctx);
}

/*!
 * Open File handle from `FILE *`.
 *
 * If \p owned is true, then the File handle will take ownership of the
 * `FILE *` instance. In other words, the `FILE *` instance will be closed when
 * the File handle is closed.
 *
 * \param file File handle
 * \param fp `FILE *` instance
 * \param owned Whether the `FILE *` instance should be owned by the File
 *              handle
 *
 * \return
 *   * #FileStatus::OK if the `FILE *` instance was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_FILE(File &file, FILE *fp, bool owned)
{
    return _file_open_FILE(&g_default_funcs, file, fp, owned);
}

/*!
 * Open File handle from a multi-byte filename.
 *
 * On Unix-like systems, \p filename is directly passed to `fopen()`. On Windows
 * systems, \p filename is converted to WCS using mb::mbs_to_wcs() before being
 * passed to `_wfopen()`.
 *
 * \param file File handle
 * \param filename MBS filename
 * \param mode Open mode (\ref FileOpenMode)
 *
 * \return
 *   * #FileStatus::OK if the file was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_FILE_filename(File &file, const char *filename,
                                   FileOpenMode mode)
{
    return _file_open_FILE_filename(&g_default_funcs, file, filename, mode);
}

/*!
 * Open File handle from a wide-character filename.
 *
 * On Unix-like systems, \p filename is converted to MBS using mb::wcs_to_mbs()
 * before being passed to `fopen()`. On Windows systems, \p filename is directly
 * passed to `_wfopen()`.
 *
 * \param file File handle
 * \param filename WCS filename
 * \param mode Open mode (\ref FileOpenMode)
 *
 * \return
 *   * #FileStatus::OK if the file was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_FILE_filename_w(File &file, const wchar_t *filename,
                                     FileOpenMode mode)
{
    return _file_open_FILE_filename_w(&g_default_funcs, file, filename, mode);
}

}
