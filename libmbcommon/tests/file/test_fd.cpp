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

#include <gmock/gmock.h>

#include <climits>

#include <fcntl.h>

#include "mbcommon/file.h"
#include "mbcommon/file/fd.h"
#include "mbcommon/file/fd_p.h"

#include "testable_file.h"

struct MockFdFileFuncs : public mb::FdFileFuncs
{
    // fcntl.h
#ifdef _WIN32
    MOCK_METHOD3(fn_wopen, int(const wchar_t *path, int flags, mode_t mode));
#else
    MOCK_METHOD3(fn_open, int(const char *path, int flags, mode_t mode));
#endif

    // sys/stat.h
    MOCK_METHOD2(fn_fstat, int(int fildes, struct stat *buf));

    // unistd.h
    MOCK_METHOD1(fn_close, int(int fd));
    MOCK_METHOD2(fn_ftruncate64, int(int fd, off_t length));
    MOCK_METHOD3(fn_lseek64, off64_t(int fd, off64_t offset, int whence));
    MOCK_METHOD3(fn_read, ssize_t(int fd, void *buf, size_t count));
    MOCK_METHOD3(fn_write, ssize_t(int fd, const void *buf, size_t count));

    struct stat _sb_regfile{};

    MockFdFileFuncs()
    {
        _sb_regfile.st_mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO;

        // Fail everything by default
#ifdef _WIN32
        ON_CALL(*this, fn_wopen(testing::_, testing::_, testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
#else
        ON_CALL(*this, fn_open(testing::_, testing::_, testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
#endif
        ON_CALL(*this, fn_fstat(testing::_, testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
        ON_CALL(*this, fn_close(testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
        ON_CALL(*this, fn_ftruncate64(testing::_, testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
        ON_CALL(*this, fn_lseek64(testing::_, testing::_, testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
        ON_CALL(*this, fn_read(testing::_, testing::_, testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
        ON_CALL(*this, fn_write(testing::_, testing::_, testing::_))
                .WillByDefault(testing::SetErrnoAndReturn(EIO, -1));
    }

    void report_as_regular_file()
    {
        ON_CALL(*this, fn_fstat(testing::_, testing::_))
                .WillByDefault(testing::DoAll(
                        testing::SetArgPointee<1>(_sb_regfile),
                        testing::Return(0)));
    }

    void open_with_success()
    {
#ifdef _WIN32
        ON_CALL(*this, fn_wopen(testing::_, testing::_, testing::_))
                .WillByDefault(testing::Return(0));
#else
        ON_CALL(*this, fn_open(testing::_, testing::_, testing::_))
                .WillByDefault(testing::Return(0));
#endif
    }
};

struct FileFdTest : testing::Test
{
    testing::NiceMock<MockFdFileFuncs> _funcs;
    TestableFile _file;
};

TEST_F(FileFdTest, OpenFilenameMbsSuccess)
{
    _funcs.report_as_regular_file();
    _funcs.open_with_success();

#ifdef _WIN32
    EXPECT_CALL(_funcs, fn_wopen(testing::_, testing::_, testing::_))
            .Times(1);
#else
    EXPECT_CALL(_funcs, fn_open(testing::_, testing::_, testing::_))
            .Times(1);
#endif

    ASSERT_EQ(mb::_file_open_fd_filename(&_funcs, _file, "x",
                                         mb::FileOpenMode::READ_ONLY),
              mb::FileStatus::OK);
}

TEST_F(FileFdTest, OpenFilenameMbsFailure)
{
    _funcs.report_as_regular_file();

#ifdef _WIN32
    EXPECT_CALL(_funcs, fn_wopen(testing::_, testing::_, testing::_))
            .Times(1);
#else
    EXPECT_CALL(_funcs, fn_open(testing::_, testing::_, testing::_))
            .Times(1);
#endif

    ASSERT_EQ(mb::_file_open_fd_filename(&_funcs, _file, "x",
                                         mb::FileOpenMode::READ_ONLY),
              mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}

TEST_F(FileFdTest, OpenFilenameMbsInvalidMode)
{
    ASSERT_EQ(mb::_file_open_fd_filename(&_funcs, _file, "x",
                                         static_cast<mb::FileOpenMode>(-1)),
              mb::FileStatus::FATAL);
    ASSERT_EQ(_file.error(), mb::FileError::INVALID_ARGUMENT);
}

TEST_F(FileFdTest, OpenFilenameWcsSuccess)
{
    _funcs.report_as_regular_file();
    _funcs.open_with_success();

#ifdef _WIN32
    EXPECT_CALL(_funcs, fn_wopen(testing::_, testing::_, testing::_))
            .Times(1);
#else
    EXPECT_CALL(_funcs, fn_open(testing::_, testing::_, testing::_))
            .Times(1);
#endif

    ASSERT_EQ(mb::_file_open_fd_filename_w(&_funcs, _file, L"x",
                                           mb::FileOpenMode::READ_ONLY),
              mb::FileStatus::OK);
}

TEST_F(FileFdTest, OpenFilenameWcsFailure)
{
    _funcs.report_as_regular_file();

#ifdef _WIN32
    EXPECT_CALL(_funcs, fn_wopen(testing::_, testing::_, testing::_))
            .Times(1);
#else
    EXPECT_CALL(_funcs, fn_open(testing::_, testing::_, testing::_))
            .Times(1);
#endif

    ASSERT_EQ(mb::_file_open_fd_filename_w(&_funcs, _file, L"x",
                                           mb::FileOpenMode::READ_ONLY),
              mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}

TEST_F(FileFdTest, OpenFilenameWcsInvalidMode)
{
    ASSERT_EQ(mb::_file_open_fd_filename_w(&_funcs, _file, L"x",
                                           static_cast<mb::FileOpenMode>(-1)),
              mb::FileStatus::FATAL);
    ASSERT_EQ(_file.error(), mb::FileError::INVALID_ARGUMENT);
}

TEST_F(FileFdTest, OpenFstatFailed)
{
    EXPECT_CALL(_funcs, fn_fstat(testing::_, testing::_))
            .Times(1);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, false),
              mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}

TEST_F(FileFdTest, OpenDirectory)
{
    struct stat sb{};
    sb.st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;

    EXPECT_CALL(_funcs, fn_fstat(testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::DoAll(testing::SetArgPointee<1>(sb),
                                     testing::Return(0)));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, false),
              mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EISDIR);
}

TEST_F(FileFdTest, OpenFile)
{
    _funcs.report_as_regular_file();

    EXPECT_CALL(_funcs, fn_fstat(testing::_, testing::_))
            .Times(1);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, false), mb::FileStatus::OK);
}

TEST_F(FileFdTest, CloseUnownedFile)
{
    _funcs.report_as_regular_file();

    // Ensure that the close callback is not called
    EXPECT_CALL(_funcs, fn_close(testing::_))
            .Times(0);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, false), mb::FileStatus::OK);
    ASSERT_EQ(_file.close(), mb::FileStatus::OK);
}

TEST_F(FileFdTest, CloseOwnedFile)
{
    _funcs.report_as_regular_file();

    // Ensure that the close callback is called
    EXPECT_CALL(_funcs, fn_close(testing::_))
            .Times(1)
            .WillOnce(testing::Return(0));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);
    ASSERT_EQ(_file.close(), mb::FileStatus::OK);
}

TEST_F(FileFdTest, CloseFailure)
{
    _funcs.report_as_regular_file();

    // Ensure that the close callback is called
    EXPECT_CALL(_funcs, fn_close(testing::_))
            .Times(1);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);
    ASSERT_EQ(_file.close(), mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}

TEST_F(FileFdTest, ReadSuccess)
{
    _funcs.report_as_regular_file();

    // Ensure that the read callback is called
    EXPECT_CALL(_funcs, fn_read(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::ReturnArg<2>());

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    char c;
    size_t n;
    ASSERT_EQ(_file.read(&c, 1, &n), mb::FileStatus::OK);
    ASSERT_EQ(n, 1u);
}

#if SIZE_MAX > SSIZE_MAX
TEST_F(FileFdTest, ReadSuccessMaxSize)
{
    _funcs.report_as_regular_file();

    // Ensure that the read callback is called
    EXPECT_CALL(_funcs, fn_read(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::ReturnArg<2>());

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    size_t n;
    ASSERT_EQ(_file.read(nullptr, static_cast<size_t>(SSIZE_MAX) + 1, &n),
              mb::FileStatus::OK);
    ASSERT_EQ(n, static_cast<size_t>(SSIZE_MAX));
}
#endif

TEST_F(FileFdTest, ReadEof)
{
    _funcs.report_as_regular_file();

    // Ensure that the read callback is called
    EXPECT_CALL(_funcs, fn_read(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::Return(0));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    char c;
    size_t n;
    ASSERT_EQ(_file.read(&c, 1, &n), mb::FileStatus::OK);
    ASSERT_EQ(n, 0u);
}

TEST_F(FileFdTest, ReadFailure)
{
    _funcs.report_as_regular_file();

    // Ensure that the read callback is called
    EXPECT_CALL(_funcs, fn_read(testing::_, testing::_, testing::_))
            .Times(1);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    char c;
    size_t n;
    ASSERT_EQ(_file.read(&c, 1, &n), mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}

TEST_F(FileFdTest, ReadFailureEINTR)
{
    _funcs.report_as_regular_file();

    // Ensure that the read callback is called
    EXPECT_CALL(_funcs, fn_read(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::SetErrnoAndReturn(EINTR, -1));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    char c;
    size_t n;
    ASSERT_EQ(_file.read(&c, 1, &n), mb::FileStatus::RETRY);
    ASSERT_EQ(_file.error(), -EINTR);
}

TEST_F(FileFdTest, WriteSuccess)
{
    _funcs.report_as_regular_file();

    // Ensure that the write callback is called
    EXPECT_CALL(_funcs, fn_write(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::ReturnArg<2>());

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    size_t n;
    ASSERT_EQ(_file.write("x", 1, &n), mb::FileStatus::OK);
    ASSERT_EQ(n, 1u);
}

#if SIZE_MAX > SSIZE_MAX
TEST_F(FileFdTest, WriteSuccessMaxSize)
{
    _funcs.report_as_regular_file();

    // Ensure that the write callback is called
    EXPECT_CALL(_funcs, fn_write(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::ReturnArg<2>());

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    size_t n;
    ASSERT_EQ(_file.write(nullptr, static_cast<size_t>(SSIZE_MAX) + 1, &n),
              mb::FileStatus::OK);
    ASSERT_EQ(n, static_cast<size_t>(SSIZE_MAX));
}
#endif

TEST_F(FileFdTest, WriteEof)
{
    _funcs.report_as_regular_file();

    // Ensure that the write callback is called
    EXPECT_CALL(_funcs, fn_write(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::Return(0));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    size_t n;
    ASSERT_EQ(_file.write("x", 1, &n), mb::FileStatus::OK);
    ASSERT_EQ(n, 0u);
}

TEST_F(FileFdTest, WriteFailure)
{
    _funcs.report_as_regular_file();

    // Ensure that the write callback is called
    EXPECT_CALL(_funcs, fn_write(testing::_, testing::_, testing::_))
            .Times(1);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    size_t n;
    ASSERT_EQ(_file.write("x", 1, &n), mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}

TEST_F(FileFdTest, WriteFailureEINTR)
{
    _funcs.report_as_regular_file();

    // Ensure that the write callback is called
    EXPECT_CALL(_funcs, fn_write(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::SetErrnoAndReturn(EINTR, -1));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    size_t n;
    ASSERT_EQ(_file.write("x", 1, &n), mb::FileStatus::RETRY);
    ASSERT_EQ(_file.error(), -EINTR);
}

TEST_F(FileFdTest, SeekSuccess)
{
    _funcs.report_as_regular_file();

    EXPECT_CALL(_funcs, fn_lseek64(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::Return(10));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    uint64_t offset;
    ASSERT_EQ(_file.seek(10, SEEK_SET, &offset), mb::FileStatus::OK);
    ASSERT_EQ(offset, 10u);
}

#define LFS_SIZE (10ULL * 1024 * 1024 * 1024)
TEST_F(FileFdTest, SeekSuccessLargeFile)
{
    _funcs.report_as_regular_file();

    EXPECT_CALL(_funcs, fn_lseek64(testing::_, testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::Return(LFS_SIZE));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    // Ensure that the types (off_t, etc.) are large enough for LFS
    uint64_t offset;
    ASSERT_EQ(_file.seek(LFS_SIZE, SEEK_SET, &offset), mb::FileStatus::OK);
    ASSERT_EQ(offset, LFS_SIZE);
}
#undef LFS_SIZE

TEST_F(FileFdTest, SeekFseekFailed)
{
    _funcs.report_as_regular_file();

    EXPECT_CALL(_funcs, fn_lseek64(testing::_, testing::_, testing::_))
            .Times(1);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    ASSERT_EQ(_file.seek(10, SEEK_SET, nullptr), mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}

TEST_F(FileFdTest, TruncateSuccess)
{
    _funcs.report_as_regular_file();

    EXPECT_CALL(_funcs, fn_ftruncate64(testing::_, testing::_))
            .Times(1)
            .WillOnce(testing::Return(0));

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    ASSERT_EQ(_file.truncate(1024), mb::FileStatus::OK);
}

TEST_F(FileFdTest, TruncateFailed)
{
    _funcs.report_as_regular_file();

    EXPECT_CALL(_funcs, fn_ftruncate64(testing::_, testing::_))
            .Times(1);

    ASSERT_EQ(mb::_file_open_fd(&_funcs, _file, 0, true), mb::FileStatus::OK);

    ASSERT_EQ(_file.truncate(1024), mb::FileStatus::FAILED);
    ASSERT_EQ(_file.error(), -EIO);
}
