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

#include "mbcommon/file.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include "mbcommon/file_p.h"
#include "mbcommon/string.h"

#define ENSURE_STATE(STATES) \
    do { \
        if (!(priv->state & (STATES))) { \
            set_error(FileError::PROGRAMMER_ERROR, \
                      "%s: Invalid state: "\
                      "expected 0x%hx, actual: 0x%hx", \
                      __func__, (STATES), priv->state); \
            priv->state = FileState::FATAL; \
            return FileStatus::FATAL; \
        } \
    } while (0)

// File documentation

/*!
 * \file mbcommon/file.h
 * \brief File abstraction API
 */

// Types documentation

/*!
 * \class File
 *
 * \brief Utility class for reading and writing files.
 */

// Return values documentation

/*!
 * \enum MbFileRet
 *
 * \brief Possible return values for functions.
 */

/*!
 * \var FileStatus::OK
 *
 * \brief Success error code
 *
 * Success error code.
 */

/*!
 * \var FileStatus::RETRY
 *
 * \brief Reattempt operation
 *
 * The operation should be reattempted.
 */

/*!
 * \var FileStatus::WARN
 *
 * \brief Warning
 *
 * The operation raised a warning. The MbFile handle can still be used although
 * the functionality may be degraded.
 */

/*!
 * \var FileStatus::FAILED
 *
 * \brief Non-fatal error
 *
 * The operation failed non-fatally. The MbFile handle can still be used for
 * further operations.
 */

/*!
 * \var FileStatus::FATAL
 *
 * \brief Fatal error
 *
 * The operation failed fatally. The MbFile handle can no longer be used for
 * further operations.
 */

/*!
 * \var FileStatus::UNSUPPORTED
 *
 * \brief Operation not supported
 *
 * The operation is not supported.
 */

// Error codes documentation

/*!
 * \namespace FileError
 *
 * \brief Possible error codes.
 */

/*!
 * \var mb::FileError::NONE
 *
 * \brief No error
 */

/*!
 * \var mb::FileError::INVALID_ARGUMENT
 *
 * \brief An invalid argument was provided
 */

/*!
 * \var mb::FileError::UNSUPPORTED
 *
 * \brief The operation is not supported
 */

/*!
 * \var mb::FileError::PROGRAMMER_ERROR
 *
 * \brief The function were called in an invalid state
 */

/*!
 * \var mb::FileError::INTERNAL_ERROR
 *
 * \brief Internal error in the library
 */

// Typedefs documentation

/*!
 * \typedef MbFileOpenCb
 *
 * \brief File open callback
 *
 * \note If a failure error code is returned. The #MbFileCloseCb callback,
 *       if registered, will be called to clean up the resources.
 *
 * \param file MbFile handle
 *
 * \return
 *   * Return #FileStatus::OK if the file was successfully opened
 *   * Return \<= #FileStatus::WARN if an error occurs
 */

/*!
 * \typedef MbFileCloseCb
 *
 * \brief File close callback
 *
 * This callback, if registered, will be called once and only once once to clean
 * up the resources, regardless of the current state. In other words, this
 * callback will be called even if a function returns #FileStatus::FATAL. If any
 * memory, file handles, or other resources need to be freed, this callback is
 * the place to do so.
 *
 * It is guaranteed that no further callbacks will be invoked after this
 * callback executes.
 *
 * \param file MbFile handle
 *
 * \return
 *   * Return #FileStatus::OK if the file was successfully closed
 *   * Return \<= #FileStatus::WARN if an error occurs
 */

/*!
 * \typedef MbFileReadCb
 *
 * \brief File read callback
 *
 * \param[in] file MbFile handle
 * \param[out] buf Buffer to read into
 * \param[in] size Buffer size
 * \param[out] bytes_read Output number of bytes that were read. 0 indicates end
 *                        of file. This parameter is guaranteed to be non-NULL.
 *
 * \return
 *   * Return #FileStatus::OK if some bytes were read or EOF is reached
 *   * Return #FileStatus::RETRY if the same operation should be reattempted
 *   * Return #FileStatus::UNSUPPORTED if the file does not support reading
 *     (Not registering a read callback has the same effect.)
 *   * Return \<= #FileStatus::WARN if an error occurs
 */

/*!
 * \typedef MbFileWriteCb
 *
 * \brief File write callback
 *
 * \param[in] file MbFile handle
 * \param[in] buf Buffer to write from
 * \param[in] size Buffer size
 * \param[out] bytes_written Output number of bytes that were written. This
 *                           parameter is guaranteed to be non-NULL.
 *
 * \return
 *   * Return #FileStatus::OK if some bytes were written
 *   * Return #FileStatus::RETRY if the same operation should be reattempted
 *   * Return #FileStatus::UNSUPPORTED if the file does not support writing
 *     (Not registering a read callback has the same effect.)
 *   * Return \<= #FileStatus::WARN if an error occurs
 */

/*!
 * \typedef MbFileSeekCb
 *
 * \brief File seek callback
 *
 * \param[in] file MbFile handle
 * \param[in] offset File position offset
 * \param[in] whence SEEK_SET, SEEK_CUR, or SEEK_END from `stdio.h`
 * \param[out] new_offset Output new file offset. This parameter is guaranteed
 *                        to be non-NULL.
 *
 * \return
 *   * Return #FileStatus::OK if the file position was successfully set
 *   * Return #FileStatus::UNSUPPORTED if the file does not support seeking
 *     (Not registering a seek callback has the same effect.)
 *   * Return \<= #FileStatus::WARN if an error occurs
 */

/*!
 * \typedef MbFileTruncateCb
 *
 * \brief File truncate callback
 *
 * \note This callback must *not* change the file position.
 *
 * \param file MbFile handle
 * \param size New size of file
 *
 * \return
 *   * Return #FileStatus::OK if the file size was successfully changed
 *   * Return #FileStatus::UNSUPPORTED if the handle source does not support
 *     truncation (Not registering a truncate callback has the same effect.)
 *   * Return \<= #FileStatus::WARN if an error occurs
 */

namespace mb
{

/*!
 * \brief Construct new File handle.
 */
File::File() : _priv_ptr(new FilePrivate())
{
    MB_PRIVATE(File);

    priv->state = FileState::NEW;
}

File::File(FilePrivate *priv_) : _priv_ptr(priv_)
{
    MB_PRIVATE(File);

    priv->state = FileState::NEW;
}

/*!
 * \brief Destroy a File handle.
 *
 * If the handle has not been closed, it will be closed. Since this is the
 * destructor, it is not possible to get the result of the file close operation.
 * To get the result of the file close operation, call File::close() manually.
 */
File::~File()
{
    MB_PRIVATE(File);

    if (priv->state != FileState::CLOSED) {
        close();
    }
}

/*!
 * \brief Set the file open callback for a File handle.
 *
 * \param open_cb File open callback
 *
 * \return
 *   * #FileStatus::OK if the callback was successfully set
 *   * #FileStatus::FATAL if the file has already been opened
 */
FileStatus File::set_open_callback(OpenCb open_cb)
{
    MB_PRIVATE(File);

    ENSURE_STATE(FileState::NEW);
    priv->open_cb = open_cb;
    return FileStatus::OK;
}

/*!
 * \brief Set the file close callback for a File handle.
 *
 * \param close_cb File close callback
 *
 * \return
 *   * #FileStatus::OK if the callback was successfully set
 *   * #FileStatus::FATAL if the file has already been opened
 */
FileStatus File::set_close_callback(CloseCb close_cb)
{
    MB_PRIVATE(File);

    ENSURE_STATE(FileState::NEW);
    priv->close_cb = close_cb;
    return FileStatus::OK;
}

/*!
 * \brief Set the file read callback for a File handle.
 *
 * \param read_cb File read callback
 *
 * \return
 *   * #FileStatus::OK if the callback was successfully set
 *   * #FileStatus::FATAL if the file has already been opened
 */
FileStatus File::set_read_callback(ReadCb read_cb)
{
    MB_PRIVATE(File);

    ENSURE_STATE(FileState::NEW);
    priv->read_cb = read_cb;
    return FileStatus::OK;
}

/*!
 * \brief Set the file write callback for a File handle.
 *
 * \param write_cb File write callback
 *
 * \return
 *   * #FileStatus::OK if the callback was successfully set
 *   * #FileStatus::FATAL if the file has already been opened
 */
FileStatus File::set_write_callback(WriteCb write_cb)
{
    MB_PRIVATE(File);

    ENSURE_STATE(FileState::NEW);
    priv->write_cb = write_cb;
    return FileStatus::OK;
}

/*!
 * \brief Set the file seek callback for a File handle.
 *
 * \param seek_cb File seek callback
 *
 * \return
 *   * #FileStatus::OK if the callback was successfully set
 *   * #FileStatus::FATAL if the file has already been opened
 */
FileStatus File::set_seek_callback(SeekCb seek_cb)
{
    MB_PRIVATE(File);

    ENSURE_STATE(FileState::NEW);
    priv->seek_cb = seek_cb;
    return FileStatus::OK;
}

/*!
 * \brief Set the file truncate callback for a File handle.
 *
 * \param truncate_cb File truncate callback
 *
 * \return
 *   * #FileStatus::OK if the callback was successfully set
 *   * #FileStatus::FATAL if the file has already been opened
 */
FileStatus File::set_truncate_callback(TruncateCb truncate_cb)
{
    MB_PRIVATE(File);

    ENSURE_STATE(FileState::NEW);
    priv->truncate_cb = truncate_cb;
    return FileStatus::OK;
}

/*!
 * \brief Set the data to provide to callbacks for a File handle.
 *
 * \param userdata User-provided data pointer for callbacks
 *
 * \return
 *   * #FileStatus::OK if the userdata was successfully set
 *   * #FileStatus::FATAL if the file has already been opened
 */
FileStatus File::set_callback_data(void *userdata)
{
    MB_PRIVATE(File);

    ENSURE_STATE(FileState::NEW);
    priv->cb_userdata = userdata;
    return FileStatus::OK;
}

/*!
 * \brief Open a File handle.
 *
 * Once the handle has been opened, the file operation functions, such as
 * File::read(), are available to use. It will no longer be possible to set
 * the callback functions for this handle.
 *
 * \return
 *   * #FileStatus::OK if there is no file open handle or if the file open
 *     handle succeeds
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus File::open()
{
    MB_PRIVATE(File);

    FileStatus ret = FileStatus::OK;

    ENSURE_STATE(FileState::NEW);

    if (priv->open_cb) {
        ret = priv->open_cb(*this, priv->cb_userdata);
    }
    if (ret == FileStatus::OK) {
        priv->state = FileState::OPENED;
    } else if (ret <= FileStatus::FATAL) {
        priv->state = FileState::FATAL;
    }

    // If the file was not successfully opened, then close it
    if (ret != FileStatus::OK && priv->close_cb) {
        priv->close_cb(*this, priv->cb_userdata);
    }

    return ret;
}

/*!
 * \brief Close a File handle.
 *
 * This function will close a File handle if it is open. Regardless of the
 * return value, the handle is closed and can no longer be used for further
 * operations.
 *
 * \return
 *   * #FileStatus::OK if no error was encountered when closing the handle.
 *   * \<= #FileStatus::WARN if the handle is opened and an error occurs while
 *     closing the file
 */
FileStatus File::close()
{
    MB_PRIVATE(File);

    FileStatus ret = FileStatus::OK;

    // Avoid double-closing or closing nothing
    if (!(priv->state & (FileState::CLOSED | FileState::NEW))) {
        if (priv->close_cb) {
            ret = priv->close_cb(*this, priv->cb_userdata);
        }

        // Don't change state to FileState::FATAL if FileStatus::FATAL is
        // returned. Otherwise, we risk double-closing the file. CLOSED and
        // FATAL are the same anyway, aside from the fact that files can be
        // closed in the latter state.
    }

    priv->state = FileState::CLOSED;

    return ret;
}

/*!
 * \brief Read from a File handle.
 *
 * Example usage:
 *
 *     char buf[10240];
 *     int ret;
 *     size_t n;
 *
 *     while ((ret = file.read(buf, sizeof(buf), &n)) == FileStatus::OK
 *             && n >= 0) {
 *         fwrite(buf, 1, n, stdout);
 *     }
 *
 *     if (ret != FileStatus::OK) {
 *         printf("Failed to read file: %s\n", file.error_string(file).c_str());
 *     }
 *
 * \param[out] buf Buffer to read into
 * \param[in] size Buffer size
 * \param[out] bytes_read Output number of bytes that were read. 0 indicates end
 *                        of file. This parameter cannot be NULL.
 *
 * \return
 *   * #FileStatus::OK if some bytes were read or EOF is reached
 *   * #FileStatus::RETRY if the same operation should be reattempted
 *   * #FileStatus::UNSUPPORTED if the handle source does not support reading
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus File::read(void *buf, size_t size, size_t *bytes_read)
{
    MB_PRIVATE(File);

    FileStatus ret = FileStatus::UNSUPPORTED;

    ENSURE_STATE(FileState::OPENED);

    if (!bytes_read) {
        set_error(FileError::PROGRAMMER_ERROR,
                  "%s: bytes_read is NULL", __func__);
        ret = FileStatus::FATAL;
    } else if (priv->read_cb) {
        ret = priv->read_cb(*this, priv->cb_userdata,
                            buf, size, bytes_read);
    } else {
        set_error(FileError::UNSUPPORTED,
                  "%s: No read callback registered", __func__);
    }
    if (ret <= FileStatus::FATAL) {
        priv->state = FileState::FATAL;
    }

    return ret;
}

/*!
 * \brief Write to a File handle.
 *
 * Example usage:
 *
 *     size_t n;
 *
 *     if (file.write(file, buf, sizeof(buf), &bytesWritten)
 *             != FileStatus::OK) {
 *         printf("Failed to write file: %s\n", file.error_string().c_str());
 *     }
 *
 * \param[in] buf Buffer to write from
 * \param[in] size Buffer size
 * \param[out] bytes_written Output number of bytes that were written. This
 *                           parameter cannot be NULL.
 *
 * \return
 *   * #FileStatus::OK if some bytes were written
 *   * #FileStatus::RETRY if the same operation should be reattempted
 *   * #FileStatus::UNSUPPORTED if the handle source does not support writing
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus File::write(const void *buf, size_t size, size_t *bytes_written)
{
    MB_PRIVATE(File);

    FileStatus ret = FileStatus::UNSUPPORTED;

    ENSURE_STATE(FileState::OPENED);

    if (!bytes_written) {
        set_error(FileError::PROGRAMMER_ERROR,
                  "%s: bytes_written is NULL", __func__);
        ret = FileStatus::FATAL;
    } else if (priv->write_cb) {
        ret = priv->write_cb(*this, priv->cb_userdata,
                             buf, size, bytes_written);
    } else {
        set_error(FileError::UNSUPPORTED,
                  "%s: No write callback registered", __func__);
    }
    if (ret <= FileStatus::FATAL) {
        priv->state = FileState::FATAL;
    }

    return ret;
}

/*!
 * \brief Set file position of a File handle.
 *
 * \param[in] offset File position offset
 * \param[in] whence SEEK_SET, SEEK_CUR, or SEEK_END from `stdio.h`
 * \param[out] new_offset Output new file offset. This parameter can be NULL.
 *
 * \return
 *   * #FileStatus::OK if the file position was successfully set
 *   * #FileStatus::UNSUPPORTED if the handle source does not support seeking
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus File::seek(int64_t offset, int whence, uint64_t *new_offset)
{
    MB_PRIVATE(File);

    FileStatus ret = FileStatus::UNSUPPORTED;
    uint64_t new_offset_temp;

    ENSURE_STATE(FileState::OPENED);

    if (priv->seek_cb) {
        ret = priv->seek_cb(*this, priv->cb_userdata,
                            offset, whence, &new_offset_temp);
    } else {
        set_error(FileError::UNSUPPORTED,
                  "%s: No seek callback registered", __func__);
    }
    if (ret == FileStatus::OK) {
        if (new_offset) {
            *new_offset = new_offset_temp;
        }
    } else if (ret <= FileStatus::FATAL) {
        priv->state = FileState::FATAL;
    }

    return ret;
}

/*!
 * \brief Truncate or extend file backed by a File handle.
 *
 * \note The file position is *not* changed after a successful call of this
 *       function. The size of the file may increase if the file position is
 *       larger than the truncated file size and File::write() is called.
 *
 * \param size New size of file
 *
 * \return
 *   * #FileStatus::OK if the file size was successfully changed
 *   * #FileStatus::UNSUPPORTED if the handle source does not support truncation
 *   * \<= #FileStatus::WARN if an error occurs
 */
FileStatus File::truncate(uint64_t size)
{
    MB_PRIVATE(File);

    FileStatus ret = FileStatus::UNSUPPORTED;

    ENSURE_STATE(FileState::OPENED);

    if (priv->truncate_cb) {
        ret = priv->truncate_cb(*this, priv->cb_userdata, size);
    } else {
        set_error(FileError::UNSUPPORTED,
                  "%s: No truncate callback registered", __func__);
    }
    if (ret <= FileStatus::FATAL) {
        priv->state = FileState::FATAL;
    }

    return ret;
}

/*!
 * \brief Get error code for a failed operation.
 *
 * \note The return value is undefined if an operation did not fail.
 *
 * \return Error code for failed operation. If \>= 0, then the file is one of
 *         the FileError entries. If \< 0, then the error code is
 *         implementation-defined (usually `-errno` or `-GetLastError()`).
 */
int File::error()
{
    MB_PRIVATE(File);

    return priv->error_code;
}

/*!
 * \brief Get error string for a failed operation.
 *
 * \note The return value is undefined if an operation did not fail.
 *
 * \return Error string for failed operation. The string contents may be
 *         undefined.
 */
std::string File::error_string()
{
    MB_PRIVATE(File);

    return priv->error_string;
}

/*!
 * \brief Set error string for a failed operation.
 *
 * \sa File::set_error_v()
 *
 * \param error_code Error code
 * \param fmt `printf()`-style format string
 * \param ... `printf()`-style format arguments
 *
 * \return FileStatus::OK if the error was successfully set or
 *         FileStatus::FAILED if an error occured
 */
FileStatus File::set_error(int error_code, const char *fmt, ...)
{
    FileStatus ret;
    va_list ap;

    va_start(ap, fmt);
    ret = set_error_v(error_code, fmt, ap);
    va_end(ap);

    return ret;
}

/*!
 * \brief Set error string for a failed operation.
 *
 * \sa File::set_error()
 *
 * \param error_code Error code
 * \param fmt `printf()`-style format string
 * \param ap `printf()`-style format arguments as a va_list
 *
 * \return FileStatus::OK if the error was successfully set or
 *         FileStatus::FAILED if an error occured
 */
FileStatus File::set_error_v(int error_code, const char *fmt, va_list ap)
{
    MB_PRIVATE(File);

    priv->error_code = error_code;
    return mb::format_v(priv->error_string, fmt, ap)
            ? FileStatus::OK : FileStatus::FAILED;
}

}
