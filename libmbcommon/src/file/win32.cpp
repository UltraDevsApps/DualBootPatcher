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

#include "mbcommon/file/win32.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "mbcommon/locale.h"

#include "mbcommon/file/callbacks.h"
#include "mbcommon/file/win32_p.h"

static_assert(sizeof(DWORD) == 4, "DWORD is not 32 bits");

/*!
 * \file mbcommon/file/win32.h
 * \brief Open file with Win32 `HANDLE` API
 */

namespace mb
{

struct RealWin32FileFuncs : public Win32FileFuncs
{
    // windows.h
    virtual BOOL fn_CloseHandle(HANDLE hObject) override
    {
        return CloseHandle(hObject);
    }

    virtual HANDLE fn_CreateFileW(LPCWSTR lpFileName,
                                  DWORD dwDesiredAccess,
                                  DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition,
                                  DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) override
    {
        return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                           lpSecurityAttributes, dwCreationDisposition,
                           dwFlagsAndAttributes, hTemplateFile);
    }

    virtual BOOL fn_ReadFile(HANDLE hFile,
                             LPVOID lpBuffer,
                             DWORD nNumberOfBytesToRead,
                             LPDWORD lpNumberOfBytesRead,
                             LPOVERLAPPED lpOverlapped) override
    {
        return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead,
                        lpNumberOfBytesRead, lpOverlapped);
    }

    virtual BOOL fn_SetEndOfFile(HANDLE hFile) override
    {
        return SetEndOfFile(hFile);
    }

    virtual BOOL fn_SetFilePointerEx(HANDLE hFile,
                                     LARGE_INTEGER liDistanceToMove,
                                     PLARGE_INTEGER lpNewFilePointer,
                                     DWORD dwMoveMethod) override
    {
        return SetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer,
                                dwMoveMethod);
    }

    virtual BOOL fn_WriteFile(HANDLE hFile,
                              LPCVOID lpBuffer,
                              DWORD nNumberOfBytesToWrite,
                              LPDWORD lpNumberOfBytesWritten,
                              LPOVERLAPPED lpOverlapped) override
    {
        return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite,
                         lpNumberOfBytesWritten, lpOverlapped);
    }
};

static RealWin32FileFuncs g_default_funcs;

static void free_ctx(Win32FileCtx *ctx)
{
    delete ctx;
}

LPCWSTR win32_error_string(Win32FileCtx *ctx, DWORD error_code)
{
    LocalFree(ctx->error);

    size_t size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,            // dwFlags
        nullptr,                                        // lpSource
        error_code,                                     // dwMessageId
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),      // dwLanguageId
        reinterpret_cast<LPWSTR>(&ctx->error),          // lpBuffer
        0,                                              // nSize
        nullptr                                         // Arguments
    );

    if (size == 0) {
        ctx->error = nullptr;
        return L"(FormatMessageW failed)";
    }

    return ctx->error;
}

static FileStatus win32_open_cb(File &file, void *userdata)
{
    Win32FileCtx *ctx = static_cast<Win32FileCtx *>(userdata);

    if (!ctx->filename.empty()) {
        ctx->handle = ctx->funcs->fn_CreateFileW(
                ctx->filename.c_str(), ctx->access, ctx->sharing, &ctx->sa,
                ctx->creation, ctx->attrib, nullptr);
        if (ctx->handle == INVALID_HANDLE_VALUE) {
            file.set_error(-GetLastError(),
                           "Failed to open file: %ls",
                           win32_error_string(ctx, GetLastError()));
            return FileStatus::FAILED;
        }
    }

    return FileStatus::OK;
}

static FileStatus win32_close_cb(File &file, void *userdata)
{
    Win32FileCtx *ctx = static_cast<Win32FileCtx *>(userdata);
    FileStatus ret = FileStatus::OK;

    if (ctx->owned && ctx->handle != INVALID_HANDLE_VALUE
            && !ctx->funcs->fn_CloseHandle(ctx->handle)) {
        file.set_error(-GetLastError(),
                       "Failed to close file: %ls",
                       win32_error_string(ctx, GetLastError()));
        ret = FileStatus::FAILED;
    }

    free_ctx(ctx);

    return ret;
}

static FileStatus win32_read_cb(File &file, void *userdata,
                                void *buf, size_t size, size_t *bytes_read)
{
    Win32FileCtx *ctx = static_cast<Win32FileCtx *>(userdata);
    DWORD n = 0;

    if (size > UINT_MAX) {
        size = UINT_MAX;
    }

    bool ret = ctx->funcs->fn_ReadFile(
        ctx->handle,    // hFile
        buf,            // lpBuffer
        size,           // nNumberOfBytesToRead
        &n,             // lpNumberOfBytesRead
        nullptr         // lpOverlapped
    );

    if (!ret) {
        file.set_error(-GetLastError(),
                       "Failed to read file: %ls",
                       win32_error_string(ctx, GetLastError()));
        return FileStatus::FAILED;
    }

    *bytes_read = n;
    return FileStatus::OK;
}

static FileStatus win32_seek_cb(File &file, void *userdata,
                                int64_t offset, int whence,
                                uint64_t *new_offset)
{
    Win32FileCtx *ctx = static_cast<Win32FileCtx *>(userdata);
    DWORD move_method;
    LARGE_INTEGER pos;
    LARGE_INTEGER new_pos;

    switch (whence) {
    case SEEK_CUR:
        move_method = FILE_CURRENT;
        break;
    case SEEK_SET:
        move_method = FILE_BEGIN;
        break;
    case SEEK_END:
        move_method = FILE_END;
        break;
    default:
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Invalid whence argument: %d", whence);
        return FileStatus::FAILED;
    }

    pos.QuadPart = offset;

    bool ret = ctx->funcs->fn_SetFilePointerEx(
        ctx->handle,    // hFile
        pos,            // liDistanceToMove
        &new_pos,       // lpNewFilePointer
        move_method     // dwMoveMethod
    );

    if (!ret) {
        file.set_error(-GetLastError(),
                       "Failed to seek file: %ls",
                       win32_error_string(ctx, GetLastError()));
        return FileStatus::FAILED;
    }

    *new_offset = new_pos.QuadPart;
    return FileStatus::OK;
}

static FileStatus win32_write_cb(File &file, void *userdata,
                                 const void *buf, size_t size,
                                 size_t *bytes_written)
{
    Win32FileCtx *ctx = static_cast<Win32FileCtx *>(userdata);
    DWORD n = 0;

    // We have to seek manually in append mode because the Win32 API has no
    // native append mode.
    if (ctx->append) {
        uint64_t pos;
        FileStatus seek_ret = win32_seek_cb(file, userdata, 0, SEEK_END, &pos);
        if (seek_ret != FileStatus::OK) {
            return seek_ret;
        }
    }

    if (size > UINT_MAX) {
        size = UINT_MAX;
    }

    bool ret = ctx->funcs->fn_WriteFile(
        ctx->handle,    // hFile
        buf,            // lpBuffer
        size,           // nNumberOfBytesToWrite
        &n,             // lpNumberOfBytesWritten
        nullptr         // lpOverlapped
    );

    if (!ret) {
        file.set_error(-GetLastError(),
                       "Failed to write file: %ls",
                       win32_error_string(ctx, GetLastError()));
        return FileStatus::FAILED;
    }

    *bytes_written = n;
    return FileStatus::OK;
}

static FileStatus win32_truncate_cb(File &file, void *userdata, uint64_t size)
{
    Win32FileCtx *ctx = static_cast<Win32FileCtx *>(userdata);
    FileStatus ret = FileStatus::OK, ret2;
    uint64_t current_pos;
    uint64_t temp;

    // Get current position
    ret2 = win32_seek_cb(file, userdata, 0, SEEK_CUR, &current_pos);
    if (ret2 != FileStatus::OK) {
        return ret2;
    }

    // Move to new position
    ret2 = win32_seek_cb(file, userdata, size, SEEK_SET, &temp);
    if (ret2 != FileStatus::OK) {
        return ret2;
    }

    // Truncate
    if (!ctx->funcs->fn_SetEndOfFile(ctx->handle)) {
        file.set_error(-GetLastError(),
                       "Failed to set EOF position: %ls",
                       win32_error_string(ctx, GetLastError()));
        ret = FileStatus::FAILED;
    }

    // Move back to initial position
    ret2 = win32_seek_cb(file, userdata, current_pos, SEEK_SET, &temp);
    if (ret2 != FileStatus::OK) {
        // We can't guarantee the file position so the handle shouldn't be used
        // anymore
        ret = FileStatus::FATAL;
    }

    return ret;
}

static Win32FileCtx * create_ctx(File &file, Win32FileFuncs *funcs)
{
    Win32FileCtx *ctx = new(std::nothrow) Win32FileCtx();
    if (!ctx) {
        file.set_error(FileError::INTERNAL_ERROR,
                       "Failed to allocate Win32FileCtx: %s",
                       strerror(errno));
        return nullptr;
    }

    ctx->funcs = funcs;

    return ctx;
}

static FileStatus open_ctx(File &file, Win32FileCtx *ctx)
{
    return file_open_callbacks(file,
                               &win32_open_cb,
                               &win32_close_cb,
                               &win32_read_cb,
                               &win32_write_cb,
                               &win32_seek_cb,
                               &win32_truncate_cb,
                               ctx);
}

static bool convert_mode(Win32FileCtx *ctx, FileOpenMode mode)
{
    DWORD access = 0;
    // Match open()/_wopen() behavior
    DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE;
    SECURITY_ATTRIBUTES sa;
    DWORD creation = 0;
    DWORD attrib = 0;
    // Win32 does not have a native append mode
    bool append = false;

    switch (mode) {
    case FileOpenMode::READ_ONLY:
        access = GENERIC_READ;
        creation = OPEN_EXISTING;
        break;
    case FileOpenMode::READ_WRITE:
        access = GENERIC_READ | GENERIC_WRITE;
        creation = OPEN_EXISTING;
        break;
    case FileOpenMode::WRITE_ONLY:
        access = GENERIC_WRITE;
        creation = CREATE_ALWAYS;
        break;
    case FileOpenMode::READ_WRITE_TRUNC:
        access = GENERIC_READ | GENERIC_WRITE;
        creation = CREATE_ALWAYS;
        break;
    case FileOpenMode::APPEND:
        access = GENERIC_WRITE;
        creation = OPEN_ALWAYS;
        append = true;
        break;
    case FileOpenMode::READ_APPEND:
        access = GENERIC_READ | GENERIC_WRITE;
        creation = OPEN_ALWAYS;
        append = true;
        break;
    default:
        return false;
    }

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = false;

    ctx->access = access;
    ctx->sharing = sharing;
    ctx->sa = sa;
    ctx->creation = creation;
    ctx->attrib = attrib;
    ctx->append = append;

    return true;
}

FileStatus _file_open_HANDLE(Win32FileFuncs *funcs, File &file, HANDLE handle,
                             bool owned, bool append)
{
    Win32FileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->handle = handle;
    ctx->owned = owned;
    ctx->append = append;

    return open_ctx(file, ctx);
}

FileStatus _file_open_HANDLE_filename(Win32FileFuncs *funcs, File &file,
                                      const char *filename,
                                      FileOpenMode mode)
{
    Win32FileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->owned = true;

    if (!mb::mbs_to_wcs(ctx->filename, filename)) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Failed to convert MBS filename or mode to WCS");
        free_ctx(ctx);
        return FileStatus::FATAL;
    }

    if (!convert_mode(ctx, mode)) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Invalid mode: %d", mode);
        free_ctx(ctx);
        return FileStatus::FATAL;
    }

    return open_ctx(file, ctx);
}

FileStatus _file_open_HANDLE_filename_w(Win32FileFuncs *funcs, File &file,
                                        const wchar_t *filename,
                                        FileOpenMode mode)
{
    Win32FileCtx *ctx = create_ctx(file, funcs);
    if (!ctx) {
        return FileStatus::FATAL;
    }

    ctx->owned = true;

    ctx->filename = filename;

    if (!convert_mode(ctx, mode)) {
        file.set_error(FileError::INVALID_ARGUMENT,
                       "Invalid mode: %d", mode);
        free_ctx(ctx);
        return FileStatus::FATAL;
    }

    return open_ctx(file, ctx);
}

/*!
 * Open File handle from Win32 `HANDLE`.
 *
 * If \p owned is true, then the File handle will take ownership of the
 * Win32 `HANDLE`. In other words, the Win32 `HANDLE` will be closed when the
 * File handle is closed.
 *
 * The \p append parameter exists because the Win32 API does not have a native
 * append mode.
 *
 * \param file File handle
 * \param handle Win32 `HANDLE`
 * \param owned Whether the Win32 `HANDLE` should be owned by the File handle
 * \param append Whether append mode should be enabled
 *
 * \return
 *   * #FileStatus::OK if the Win32 `HANDLE` was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_HANDLE(File &file, HANDLE handle, bool owned, bool append)
{
    return _file_open_HANDLE(&g_default_funcs, file, handle, owned, append);
}

/*!
 * Open File handle from a multi-byte filename.
 *
 * \p filename is converted to WCS using mb::mbs_to_wcs() before being used.
 *
 * \param file File handle
 * \param filename MBS filename
 * \param mode Open mode (\ref FileOpenMode)
 *
 * \return
 *   * #FileStatus::OK if the file was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_HANDLE_filename(File &file, const char *filename,
                                     FileOpenMode mode)
{
    return _file_open_HANDLE_filename(&g_default_funcs, file, filename, mode);
}

/*!
 * Open File handle from a wide-character filename.
 *
 * \p filename is used directly without any conversions.
 *
 * \param file File handle
 * \param filename WCS filename
 * \param mode Open mode (\ref FileOpenMode)
 *
 * \return
 *   * #FileStatus::OK if the file was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_HANDLE_filename_w(File &file, const wchar_t *filename,
                                       FileOpenMode mode)
{
    return _file_open_HANDLE_filename_w(&g_default_funcs, file, filename, mode);
}

}
