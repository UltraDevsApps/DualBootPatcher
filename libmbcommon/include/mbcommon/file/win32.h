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

#include "mbcommon/file/filename.h"

#include <windows.h>

namespace mb
{

MB_EXPORT FileStatus file_open_HANDLE(File &file,
                                      HANDLE handle, bool owned, bool append);

MB_EXPORT FileStatus file_open_HANDLE_filename(File &file,
                                               const char *filename,
                                               FileOpenMode mode);
MB_EXPORT FileStatus file_open_HANDLE_filename_w(File &file,
                                                 const wchar_t *filename,
                                                 FileOpenMode mode);

}
