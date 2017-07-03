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

#include "mbcommon/file/posix.h"

/*! \cond INTERNAL */
namespace mb
{

struct PosixFileFuncs
{
    // sys/stat.h
    virtual int fn_fstat(int fildes, struct stat *buf) = 0;

    // stdio.h
    virtual int fn_fclose(FILE *stream) = 0;
    virtual int fn_ferror(FILE *stream) = 0;
    virtual int fn_fileno(FILE *stream) = 0;
#ifdef _WIN32
    virtual FILE * fn_wfopen(const wchar_t *filename, const wchar_t *mode) = 0;
#else
    virtual FILE * fn_fopen(const char *path, const char *mode) = 0;
#endif
    virtual size_t fn_fread(void *ptr, size_t size, size_t nmemb,
                            FILE *stream) = 0;
    virtual int fn_fseeko(FILE *stream, off_t offset, int whence) = 0;
    virtual off_t fn_ftello(FILE *stream) = 0;
    virtual size_t fn_fwrite(const void *ptr, size_t size, size_t nmemb,
                             FILE *stream) = 0;

    // unistd.h
    virtual int fn_ftruncate64(int fd, off_t length) = 0;
};

struct PosixFileCtx
{
    FILE *fp;
    bool owned;
#ifdef _WIN32
    std::wstring filename;
    const wchar_t *mode;
#else
    std::string filename;
    const char *mode;
#endif

    bool can_seek;

    PosixFileFuncs *funcs;
};

FileStatus _file_open_FILE(PosixFileFuncs *funcs, File &file, FILE *fp,
                           bool owned);

FileStatus _file_open_FILE_filename(PosixFileFuncs *funcs, File &file,
                                    const char *filename,
                                    FileOpenMode mode);
FileStatus _file_open_FILE_filename_w(PosixFileFuncs *funcs, File &file,
                                      const wchar_t *filename,
                                      FileOpenMode mode);

}
/*! \endcond */
