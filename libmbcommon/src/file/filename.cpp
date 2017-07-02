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

#include "mbcommon/file/filename.h"

#if defined(_WIN32)
#  include "mbcommon/file/win32_p.h"
#elif defined(__ANDROID__)
#  include "mbcommon/file/fd_p.h"
#else
#  include "mbcommon/file/posix_p.h"
#endif

/*!
 * \file mbcommon/file/filename.h
 * \brief Open file with filename
 *
 * * On Windows systems, the mbcommon/file/win32.h API is used
 * * On Android systems, the mbcommon/file/fd.h API is used
 * * On other Unix-like systems, the mbcommon/file/posix.h API is used
 */

/*!
 * \enum FileOpenMode
 *
 * \brief Possible file open modes
 */

/*!
 * \var FileOpenMode::READ_ONLY
 *
 * \brief Open file for reading.
 *
 * The file pointer is set to the beginning of the file.
 */

/*!
 * \var FileOpenMode::READ_WRITE
 *
 * \brief Open file for reading and writing.
 *
 * The file pointer is set to the beginning of the file.
 */

/*!
 * \var FileOpenMode::WRITE_ONLY
 *
 * \brief Truncate file and open for writing.
 *
 * The file pointer is set to the beginning of the file.
 */

/*!
 * \var FileOpenMode::READ_WRITE_TRUNC
 *
 * \brief Truncate file and open for reading and writing.
 *
 * The file pointer is set to the beginning of the file.
 */

/*!
 * \var FileOpenMode::APPEND
 *
 * \brief Open file for appending.
 *
 * The file pointer is set to the end of the file.
 */

/*!
 * \var FileOpenMode::READ_APPEND
 *
 * \brief Open file for reading and appending.
 *
 * The file pointer is initially set to the beginning of the file, but writing
 * always occurs at the end of the file.
 */

namespace mb
{

typedef FileStatus (*OpenFunc)(File &file,
                               const char *filename, FileOpenMode mode);
typedef FileStatus (*OpenWFunc)(File &file,
                                const wchar_t *filename, FileOpenMode mode);

#define SET_FUNCTIONS(TYPE) \
    static OpenFunc open_func = \
        file_open_ ## TYPE ## _filename; \
    static OpenWFunc open_w_func = \
        file_open_ ## TYPE ## _filename_w;

#if defined(_WIN32)
SET_FUNCTIONS(HANDLE)
#elif defined(__ANDROID__)
SET_FUNCTIONS(fd)
#else
SET_FUNCTIONS(FILE)
#endif

/*!
 * Open File handle from a multi-byte filename.
 *
 * On Unix-like systems, \p filename is used directly. On Windows systems,
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
FileStatus file_open_filename(File &file,
                              const char *filename, FileOpenMode mode)
{
    return open_func(file, filename, mode);
}

/*!
 * Open File handle from a wide-character filename.
 *
 * On Unix-like systems, \p filename is converted to MBS using mb::wcs_to_mbs()
 * before begin used. On Windows systems, \p filename is used directly.
 *
 * \param file File handle
 * \param filename WCS filename
 * \param mode Open mode (\ref FileOpenMode)
 *
 * \return
 *   * #FileStatus::OK if the file was successfully opened
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus file_open_filename_w(File &file,
                                const wchar_t *filename, FileOpenMode mode)
{
    return open_w_func(file, filename, mode);
}

}
