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

#include "mbcommon/guard_p.h"

#include <windows.h>

#include "mbcommon/file/win32.h"

/*! \cond INTERNAL */
namespace mb
{

struct Win32FileFuncs
{
    // windows.h
    virtual BOOL fn_CloseHandle(HANDLE hObject) = 0;
    virtual HANDLE fn_CreateFileW(LPCWSTR lpFileName,
                                  DWORD dwDesiredAccess,
                                  DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition,
                                  DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) = 0;
    virtual BOOL fn_ReadFile(HANDLE hFile,
                             LPVOID lpBuffer,
                             DWORD nNumberOfBytesToRead,
                             LPDWORD lpNumberOfBytesRead,
                             LPOVERLAPPED lpOverlapped) = 0;
    virtual BOOL fn_SetEndOfFile(HANDLE hFile) = 0;
    virtual BOOL fn_SetFilePointerEx(HANDLE hFile,
                                     LARGE_INTEGER liDistanceToMove,
                                     PLARGE_INTEGER lpNewFilePointer,
                                     DWORD dwMoveMethod) = 0;
    virtual BOOL fn_WriteFile(HANDLE hFile,
                              LPCVOID lpBuffer,
                              DWORD nNumberOfBytesToWrite,
                              LPDWORD lpNumberOfBytesWritten,
                              LPOVERLAPPED lpOverlapped) = 0;
};

struct Win32FileCtx
{
    HANDLE handle;
    bool owned;
    std::wstring filename;
    LPWSTR error;

    // For CreateFileW
    DWORD access;
    DWORD sharing;
    SECURITY_ATTRIBUTES sa;
    DWORD creation;
    DWORD attrib;

    bool append;

    Win32FileFuncs *funcs;
};

FileStatus _file_open_HANDLE(Win32FileFuncs *funcs, File &file, HANDLE handle,
                             bool owned, bool append);

FileStatus _file_open_HANDLE_filename(Win32FileFuncs *funcs, File &file,
                                      const char *filename,
                                      FileOpenMode mode);
FileStatus _file_open_HANDLE_filename_w(Win32FileFuncs *funcs, File &file,
                                        const wchar_t *filename,
                                        FileOpenMode mode);

}
/*! \endcond */
