/*
 * Copyright (C) 2016  Andrew Gunnerson <andrewgunnerson@gmail.com>
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

#include <gtest/gtest.h>

#include <cinttypes>

#include "mbcommon/file.h"
#include "mbcommon/file_p.h"
#include "mbcommon/string.h"

#include "file/testable_file.h"

struct FileTest : testing::Test
{
    static constexpr size_t INITIAL_BUF_SIZE = 1024;

    std::vector<unsigned char> _buf;
    size_t _position = 0;

    // Callback counters
    int _n_open = 0;
    int _n_close = 0;
    int _n_read = 0;
    int _n_write = 0;
    int _n_seek = 0;
    int _n_truncate = 0;

    void set_all_callbacks(TestableFile &file)
    {
        ASSERT_EQ(file.set_open_callback(&_open_cb), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->open_cb, &_open_cb);
        ASSERT_EQ(file.set_close_callback(&_close_cb), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->close_cb, &_close_cb);
        ASSERT_EQ(file.set_read_callback(&_read_cb), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->read_cb, &_read_cb);
        ASSERT_EQ(file.set_write_callback(&_write_cb), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->write_cb, &_write_cb);
        ASSERT_EQ(file.set_seek_callback(&_seek_cb), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->seek_cb, &_seek_cb);
        ASSERT_EQ(file.set_truncate_callback(&_truncate_cb), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->truncate_cb, &_truncate_cb);
        ASSERT_EQ(file.set_callback_data(this), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->cb_userdata, this);
    }

    static mb::FileStatus _open_cb(mb::File &file, void *userdata)
    {
        (void) file;

        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_open;

        // Generate some data
        for (size_t i = 0; i < INITIAL_BUF_SIZE; ++i) {
            test->_buf.push_back('a' + (i % 26));
        }
        return mb::FileStatus::OK;
    }

    static mb::FileStatus _close_cb(mb::File &file, void *userdata)
    {
        (void) file;

        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_close;

        test->_buf.clear();
        return mb::FileStatus::OK;
    }

    static mb::FileStatus _read_cb(mb::File &file, void *userdata,
                                   void *buf, size_t size,
                                   size_t *bytes_read)
    {
        (void) file;

        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_read;

        size_t empty = test->_buf.size() - test->_position;
        uint64_t n = std::min<uint64_t>(empty, size);
        memcpy(buf, test->_buf.data() + test->_position, n);
        test->_position += n;
        *bytes_read = n;

        return mb::FileStatus::OK;
    }

    static mb::FileStatus _write_cb(mb::File &file, void *userdata,
                                    const void *buf, size_t size,
                                    size_t *bytes_written)
    {
        (void) file;

        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_write;

        size_t required = test->_position + size;
        if (required > test->_buf.size()) {
            test->_buf.resize(required);
        }

        memcpy(test->_buf.data() + test->_position, buf, size);
        test->_position += size;
        *bytes_written = size;

        return mb::FileStatus::OK;
    }

    static mb::FileStatus _seek_cb(mb::File &file, void *userdata,
                                   int64_t offset, int whence,
                                   uint64_t *new_offset)
    {
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_seek;

        switch (whence) {
        case SEEK_SET:
            if (offset < 0) {
                file.set_error(mb::FileError::INVALID_ARGUMENT,
                               "Invalid SEET_SET offset %" PRId64,
                               offset);
                return mb::FileStatus::FAILED;
            }
            *new_offset = test->_position = offset;
            break;
        case SEEK_CUR:
            if (offset < 0 && static_cast<size_t>(-offset) > test->_position) {
                file.set_error(mb::FileError::INVALID_ARGUMENT,
                               "Invalid SEEK_CUR offset %" PRId64
                               " for position %" MB_PRIzu,
                               offset, test->_position);
                return mb::FileStatus::FAILED;
            }
            *new_offset = test->_position += offset;
            break;
        case SEEK_END:
            if (offset < 0 && static_cast<size_t>(-offset) > test->_buf.size()) {
                file.set_error(mb::FileError::INVALID_ARGUMENT,
                               "Invalid SEEK_END offset %" PRId64
                               " for file of size %" MB_PRIzu,
                               offset, test->_buf.size());
                return mb::FileStatus::FAILED;
            }
            *new_offset = test->_position = test->_buf.size() + offset;
            break;
        default:
            file.set_error(mb::FileError::INVALID_ARGUMENT,
                           "Invalid whence argument: %d", whence);
            return mb::FileStatus::FAILED;
        }

        return mb::FileStatus::OK;
    }

    static mb::FileStatus _truncate_cb(mb::File &file, void *userdata,
                                       uint64_t size)
    {
        (void) file;

        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_truncate;

        test->_buf.resize(size);
        return mb::FileStatus::OK;
    }
};

constexpr size_t FileTest::INITIAL_BUF_SIZE;

TEST_F(FileTest, CheckInitialValues)
{
    TestableFile file;

    ASSERT_EQ(file._priv_func()->state, mb::FileState::NEW);
    ASSERT_EQ(file._priv_func()->open_cb, nullptr);
    ASSERT_EQ(file._priv_func()->close_cb, nullptr);
    ASSERT_EQ(file._priv_func()->read_cb, nullptr);
    ASSERT_EQ(file._priv_func()->write_cb, nullptr);
    ASSERT_EQ(file._priv_func()->seek_cb, nullptr);
    ASSERT_EQ(file._priv_func()->truncate_cb, nullptr);
    ASSERT_EQ(file._priv_func()->cb_userdata, nullptr);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::NONE);
    ASSERT_TRUE(file._priv_func()->error_string.empty());
}

TEST_F(FileTest, CheckStatesNormal)
{
    TestableFile file;

    ASSERT_EQ(file._priv_func()->state, mb::FileState::NEW);

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Close file
    ASSERT_EQ(file.close(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, FreeNewFileWithRegisteredCallbacks)
{
    {
        TestableFile file;
        set_all_callbacks(file);
    }

    // The close callback should not have been called because nothing was opened
    ASSERT_EQ(_n_close, 0);
}

TEST_F(FileTest, FreeOpenedFile)
{
    {
        TestableFile file;

        // Set callbacks
        set_all_callbacks(file);

        // Open file
        ASSERT_EQ(file.open(), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
        ASSERT_EQ(_n_open, 1);
    }

    // Ensure that the close callback was called
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, FreeClosedFile)
{
    {
        TestableFile file;

        // Set callbacks
        set_all_callbacks(file);

        // Open file
        ASSERT_EQ(file.open(), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
        ASSERT_EQ(_n_open, 1);

        // Close file
        ASSERT_EQ(file.close(), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
        ASSERT_EQ(_n_close, 1);
    }

    // Ensure that the close callback was not called again
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, FreeFatalFile)
{
    {
        TestableFile file;

        // Set callbacks
        set_all_callbacks(file);

        // Set read callback to return fatal error
        auto fatal_open_cb = [](mb::File &file, void *userdata,
                                void *buf, size_t size,
                                size_t *bytes_read) -> mb::FileStatus {
            (void) file;
            (void) buf;
            (void) size;
            (void) bytes_read;
            FileTest *test = static_cast<FileTest *>(userdata);
            ++test->_n_read;
            return mb::FileStatus::FATAL;
        };
        ASSERT_EQ(file.set_read_callback(fatal_open_cb), mb::FileStatus::OK);

        // Open file
        ASSERT_EQ(file.open(), mb::FileStatus::OK);
        ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
        ASSERT_EQ(_n_open, 1);

        // Read file
        char c;
        size_t n;
        ASSERT_EQ(file.read(&c, 1, &n), mb::FileStatus::FATAL);
        ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
        ASSERT_EQ(_n_read, 1);
    }

    // Ensure that the close callback was called
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, SetCallbacksInNonNewState)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(_buf.size(), 0u);
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_buf.size(), INITIAL_BUF_SIZE);
    ASSERT_EQ(_n_open, 1);

    ASSERT_EQ(file.set_open_callback(nullptr), mb::FileStatus::FATAL);
    ASSERT_NE(file._priv_func()->open_cb, nullptr);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("set_open_callback"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);

    ASSERT_EQ(file.set_close_callback(nullptr), mb::FileStatus::FATAL);
    ASSERT_NE(file._priv_func()->close_cb, nullptr);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("set_close_callback"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);

    ASSERT_EQ(file.set_read_callback(nullptr), mb::FileStatus::FATAL);
    ASSERT_NE(file._priv_func()->read_cb, nullptr);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("set_read_callback"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);

    ASSERT_EQ(file.set_write_callback(nullptr), mb::FileStatus::FATAL);
    ASSERT_NE(file._priv_func()->write_cb, nullptr);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("set_write_callback"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);

    ASSERT_EQ(file.set_seek_callback(nullptr), mb::FileStatus::FATAL);
    ASSERT_NE(file._priv_func()->seek_cb, nullptr);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("set_seek_callback"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);

    ASSERT_EQ(file.set_truncate_callback(nullptr), mb::FileStatus::FATAL);
    ASSERT_NE(file._priv_func()->truncate_cb, nullptr);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("set_truncate_callback"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);

    ASSERT_EQ(file.set_callback_data(nullptr), mb::FileStatus::FATAL);
    ASSERT_NE(file._priv_func()->cb_userdata, nullptr);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("set_callback_data"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);
}

TEST_F(FileTest, OpenReturnNonFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set open callback
    auto open_cb = [](mb::File &file, void *userdata) -> mb::FileStatus {
        (void) file;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_open;
        return mb::FileStatus::FAILED;
    };
    ASSERT_EQ(file.set_open_callback(open_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::FAILED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::NEW);
    ASSERT_EQ(_n_open, 1);

    // Reopen file
    ASSERT_EQ(file.set_open_callback(&_open_cb), mb::FileStatus::OK);
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 2);

    // The close callback should have been called to clean up resources
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, OpenReturnFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set open callback
    auto open_cb = [](mb::File &file, void *userdata) -> mb::FileStatus {
        (void) file;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_open;
        return mb::FileStatus::FATAL;
    };
    ASSERT_EQ(file.set_open_callback(open_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(_n_open, 1);

    // The close callback should have been called to clean up resources
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, OpenFileTwice)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Open again
    ASSERT_EQ(file.open(), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("open"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);
    ASSERT_EQ(_n_open, 1);
}

TEST_F(FileTest, OpenNoCallback)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Clear open callback
    ASSERT_EQ(file.set_open_callback(nullptr), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 0);
}

TEST_F(FileTest, CloseNewFile)
{
    TestableFile file;

    set_all_callbacks(file);

    ASSERT_EQ(file.close(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
    ASSERT_EQ(_n_close, 0);
}

TEST_F(FileTest, CloseFileTwice)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Close file
    ASSERT_EQ(file.close(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
    ASSERT_EQ(_n_close, 1);

    // Close file again
    ASSERT_EQ(file.close(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, CloseNoCallback)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Clear close callback
    file.set_close_callback(nullptr);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Close file
    ASSERT_EQ(file.close(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
    ASSERT_EQ(_n_close, 0);
}

TEST_F(FileTest, CloseReturnNonFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set open callback
    auto close_cb = [](mb::File &file, void *userdata) -> mb::FileStatus {
        (void) file;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_close;
        return mb::FileStatus::FAILED;
    };
    ASSERT_EQ(file.set_close_callback(close_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Close file
    ASSERT_EQ(file.close(), mb::FileStatus::FAILED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, CloseReturnFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set open callback
    auto close_cb = [](mb::File &file, void *userdata) -> mb::FileStatus {
        (void) file;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_close;
        return mb::FileStatus::FATAL;
    };
    ASSERT_EQ(file.set_close_callback(close_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Close file
    ASSERT_EQ(file.close(), mb::FileStatus::FATAL);
    // File::close() always results in the state changing to CLOSED
    ASSERT_EQ(file._priv_func()->state, mb::FileState::CLOSED);
    ASSERT_EQ(_n_close, 1);
}

TEST_F(FileTest, ReadCallbackCalled)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Read from file
    char buf[10];
    size_t n;
    ASSERT_EQ(file.read(buf, sizeof(buf), &n), mb::FileStatus::OK);
    ASSERT_EQ(n, sizeof(buf));
    ASSERT_EQ(memcmp(buf, _buf.data(), sizeof(buf)), 0);
    ASSERT_EQ(_n_read, 1);
}

TEST_F(FileTest, ReadInWrongState)
{
    TestableFile file;

    // Read from file
    char c;
    size_t n;
    ASSERT_EQ(file.read(&c, 1, &n), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("read"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);
    ASSERT_EQ(_n_read, 0);
}

TEST_F(FileTest, ReadWithNullBytesReadParam)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Read from file
    char c;
    ASSERT_EQ(file.read(&c, 1, nullptr), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("read"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("is NULL"), std::string::npos);
    ASSERT_EQ(_n_read, 0);
}

TEST_F(FileTest, ReadNoCallback)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Clear read callback
    file.set_read_callback(nullptr);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Read from file
    char c;
    size_t n;
    ASSERT_EQ(file.read(&c, 1, &n), mb::FileStatus::UNSUPPORTED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::UNSUPPORTED);
    ASSERT_NE(file._priv_func()->error_string.find("read"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("read callback"), std::string::npos);
    ASSERT_EQ(_n_read, 0);
}

TEST_F(FileTest, ReadReturnNonFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set read callback
    auto read_cb = [](mb::File &file, void *userdata,
                      void *buf, size_t size,
                      size_t *bytes_read) -> mb::FileStatus {
        (void) file;
        (void) buf;
        (void) size;
        (void) bytes_read;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_read;
        return mb::FileStatus::FAILED;
    };
    ASSERT_EQ(file.set_read_callback(read_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Read from file
    char c;
    size_t n;
    ASSERT_EQ(file.read(&c, 1, &n), mb::FileStatus::FAILED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_read, 1);
}

TEST_F(FileTest, ReadReturnFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set read callback
    auto read_cb = [](mb::File &file, void *userdata,
                      void *buf, size_t size,
                      size_t *bytes_read) -> mb::FileStatus {
        (void) file;
        (void) buf;
        (void) size;
        (void) bytes_read;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_read;
        return mb::FileStatus::FATAL;
    };
    ASSERT_EQ(file.set_read_callback(read_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Read from file
    char c;
    size_t n;
    ASSERT_EQ(file.read(&c, 1, &n), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(_n_read, 1);
}

TEST_F(FileTest, WriteCallbackCalled)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Write to file
    char buf[] = "Hello, world!";
    size_t size = strlen(buf);
    size_t n;
    ASSERT_EQ(file.write(buf, size, &n), mb::FileStatus::OK);
    ASSERT_EQ(n, size);
    ASSERT_EQ(memcmp(buf, _buf.data(), size), 0);
    ASSERT_EQ(_n_write, 1);
}

TEST_F(FileTest, WriteInWrongState)
{
    TestableFile file;

    // Write to file
    char c;
    size_t n;
    ASSERT_EQ(file.write(&c, 1, &n), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("write"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);
    ASSERT_EQ(_n_write, 0);
}

TEST_F(FileTest, WriteWithNullBytesReadParam)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Write to file
    ASSERT_EQ(file.write("x", 1, nullptr), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("write"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("is NULL"), std::string::npos);
    ASSERT_EQ(_n_write, 0);
}

TEST_F(FileTest, WriteNoCallback)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Clear write callback
    file.set_write_callback(nullptr);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Write to file
    size_t n;
    ASSERT_EQ(file.write("x", 1, &n), mb::FileStatus::UNSUPPORTED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::UNSUPPORTED);
    ASSERT_NE(file._priv_func()->error_string.find("write"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("write callback"), std::string::npos);
    ASSERT_EQ(_n_write, 0);
}

TEST_F(FileTest, WriteReturnNonFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set write callback
    auto write_cb = [](mb::File &file, void *userdata,
                       const void *buf, size_t size,
                       size_t *bytes_written) -> mb::FileStatus {
        (void) file;
        (void) buf;
        (void) size;
        (void) bytes_written;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_write;
        return mb::FileStatus::FAILED;
    };
    ASSERT_EQ(file.set_write_callback(write_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Write to file
    size_t n;
    ASSERT_EQ(file.write("x", 1, &n), mb::FileStatus::FAILED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_write, 1);
}

TEST_F(FileTest, WriteReturnFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set write callback
    auto write_cb = [](mb::File &file, void *userdata,
                      const void *buf, size_t size,
                      size_t *bytes_written) -> mb::FileStatus {
        (void) file;
        (void) buf;
        (void) size;
        (void) bytes_written;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_write;
        return mb::FileStatus::FATAL;
    };
    ASSERT_EQ(file.set_write_callback(write_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Write to file
    char c;
    size_t n;
    ASSERT_EQ(file.write(&c, 1, &n), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(_n_write, 1);
}

TEST_F(FileTest, SeekCallbackCalled)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Seek file
    uint64_t pos;
    ASSERT_EQ(file.seek(0, SEEK_END, &pos), mb::FileStatus::OK);
    ASSERT_EQ(pos, _buf.size());
    ASSERT_EQ(pos, _position);
    ASSERT_EQ(_n_seek, 1);

    // Seek again with NULL offset output parameter
    ASSERT_EQ(file.seek(-10, SEEK_END, nullptr), mb::FileStatus::OK);
    ASSERT_EQ(_position, _buf.size() - 10);
    ASSERT_EQ(_n_seek, 2);
}

TEST_F(FileTest, SeekInWrongState)
{
    TestableFile file;

    // Seek file
    ASSERT_EQ(file.seek(0, SEEK_END, nullptr), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("seek"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);
    ASSERT_EQ(_n_seek, 0);
}

TEST_F(FileTest, SeekNoCallback)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Clear seek callback
    file.set_seek_callback(nullptr);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Seek file
    ASSERT_EQ(file.seek(0, SEEK_END, nullptr), mb::FileStatus::UNSUPPORTED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::UNSUPPORTED);
    ASSERT_NE(file._priv_func()->error_string.find("seek"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("seek callback"), std::string::npos);
    ASSERT_EQ(_n_seek, 0);
}

TEST_F(FileTest, SeekReturnNonFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set seek callback
    auto seek_cb = [](mb::File &file, void *userdata,
                      int64_t offset, int whence,
                      uint64_t *new_offset) -> mb::FileStatus {
        (void) file;
        (void) offset;
        (void) whence;
        (void) new_offset;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_seek;
        return mb::FileStatus::FAILED;
    };
    ASSERT_EQ(file.set_seek_callback(seek_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Seek file
    ASSERT_EQ(file.seek(0, SEEK_END, nullptr), mb::FileStatus::FAILED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_seek, 1);
}

TEST_F(FileTest, SeekReturnFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set seek callback
    auto seek_cb = [](mb::File &file, void *userdata,
                      int64_t offset, int whence,
                      uint64_t *new_offset) -> mb::FileStatus {
        (void) file;
        (void) offset;
        (void) whence;
        (void) new_offset;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_seek;
        return mb::FileStatus::FATAL;
    };
    ASSERT_EQ(file.set_seek_callback(seek_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Seek file
    ASSERT_EQ(file.seek(0, SEEK_END, nullptr), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(_n_seek, 1);
}

TEST_F(FileTest, TruncateCallbackCalled)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Truncate file
    ASSERT_EQ(file.truncate(INITIAL_BUF_SIZE / 2), mb::FileStatus::OK);
    ASSERT_EQ(_n_truncate, 1);
}

TEST_F(FileTest, TruncateInWrongState)
{
    TestableFile file;

    // Truncate file
    ASSERT_EQ(file.truncate(INITIAL_BUF_SIZE + 1), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::PROGRAMMER_ERROR);
    ASSERT_NE(file._priv_func()->error_string.find("truncate"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("Invalid state"), std::string::npos);
    ASSERT_EQ(_n_truncate, 0);
}

TEST_F(FileTest, TruncateNoCallback)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Clear truncate callback
    file.set_truncate_callback(nullptr);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Truncate file
    ASSERT_EQ(file.truncate(INITIAL_BUF_SIZE + 1), mb::FileStatus::UNSUPPORTED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::UNSUPPORTED);
    ASSERT_NE(file._priv_func()->error_string.find("truncate"), std::string::npos);
    ASSERT_NE(file._priv_func()->error_string.find("truncate callback"), std::string::npos);
    ASSERT_EQ(_n_truncate, 0);
}

TEST_F(FileTest, TruncateReturnNonFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set truncate callback
    auto truncate_cb = [](mb::File &file, void *userdata,
                          uint64_t size) -> mb::FileStatus {
        (void) file;
        (void) size;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_truncate;
        return mb::FileStatus::FAILED;
    };
    ASSERT_EQ(file.set_truncate_callback(truncate_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Truncate file
    ASSERT_EQ(file.truncate(INITIAL_BUF_SIZE + 1), mb::FileStatus::FAILED);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_truncate, 1);
}

TEST_F(FileTest, TruncateReturnFatalFailure)
{
    TestableFile file;

    // Set callbacks
    set_all_callbacks(file);

    // Set truncate callback
    auto truncate_cb = [](mb::File &file, void *userdata,
                          uint64_t size) -> mb::FileStatus {
        (void) file;
        (void) size;
        FileTest *test = static_cast<FileTest *>(userdata);
        ++test->_n_truncate;
        return mb::FileStatus::FATAL;
    };
    ASSERT_EQ(file.set_truncate_callback(truncate_cb), mb::FileStatus::OK);

    // Open file
    ASSERT_EQ(file.open(), mb::FileStatus::OK);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::OPENED);
    ASSERT_EQ(_n_open, 1);

    // Truncate file
    ASSERT_EQ(file.truncate(INITIAL_BUF_SIZE + 1), mb::FileStatus::FATAL);
    ASSERT_EQ(file._priv_func()->state, mb::FileState::FATAL);
    ASSERT_EQ(_n_truncate, 1);
}

TEST_F(FileTest, SetError)
{
    TestableFile file;

    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::NONE);
    ASSERT_TRUE(file._priv_func()->error_string.empty());

    ASSERT_EQ(file.set_error(mb::FileError::INTERNAL_ERROR,
                             "%s, %s!", "Hello", "world"), mb::FileStatus::OK);

    ASSERT_EQ(file._priv_func()->error_code, mb::FileError::INTERNAL_ERROR);
    ASSERT_EQ(file._priv_func()->error_string, "Hello, world!");
    ASSERT_EQ(file.error(), file._priv_func()->error_code);
    ASSERT_EQ(file.error_string(), file._priv_func()->error_string);
}
