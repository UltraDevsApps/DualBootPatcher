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

#include "mbcommon/file/callbacks.h"

/*!
 * \file mbcommon/file/callbacks.h
 * \brief Open file with callbacks
 */

namespace mb
{

/*!
 * Open File handle with callbacks.
 *
 * This is a wrapper function around the `File::set_*_callback()` functions.
 *
 * \param file File handle
 * \param open_cb File open callback
 * \param close_cb File close callback
 * \param read_cb File read callback
 * \param write_cb File write callback
 * \param seek_cb File seek callback
 * \param truncate_cb File truncate callback
 * \param userdata Data pointer to pass to callbacks
 *
 * \return The minimum return value of the `File::set_*_callback()` functions,
 *         File::set_callback_data(), and File::open()
 */
FileStatus file_open_callbacks(File &file,
                               File::OpenCb open_cb,
                               File::CloseCb close_cb,
                               File::ReadCb read_cb,
                               File::WriteCb write_cb,
                               File::SeekCb seek_cb,
                               File::TruncateCb truncate_cb,
                               void *userdata)
{
    FileStatus ret = FileStatus::OK;
    FileStatus ret2;

    ret2 = file.set_open_callback(open_cb);
    if (ret2 < ret) {
        ret = ret2;
    }

    ret2 = file.set_close_callback(close_cb);
    if (ret2 < ret) {
        ret = ret2;
    }

    ret2 = file.set_read_callback(read_cb);
    if (ret2 < ret) {
        ret = ret2;
    }

    ret2 = file.set_write_callback(write_cb);
    if (ret2 < ret) {
        ret = ret2;
    }

    ret2 = file.set_seek_callback(seek_cb);
    if (ret2 < ret) {
        ret = ret2;
    }

    ret2 = file.set_truncate_callback(truncate_cb);
    if (ret2 < ret) {
        ret = ret2;
    }

    ret2 = file.set_callback_data(userdata);
    if (ret2 < ret) {
        ret = ret2;
    }

    ret2 = file.open();
    if (ret2 < ret) {
        ret = ret2;
    }

    return ret;
}

}
