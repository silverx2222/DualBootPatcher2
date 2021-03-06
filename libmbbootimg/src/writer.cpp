/*
 * Copyright (C) 2017-2018  Andrew Gunnerson <andrewgunnerson@gmail.com>
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

#include "mbbootimg/writer.h"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "mbcommon/file.h"
#include "mbcommon/file/standard.h"
#include "mbcommon/finally.h"

#include "mbbootimg/entry.h"
#include "mbbootimg/format/android_writer_p.h"
#include "mbbootimg/format/loki_writer_p.h"
#include "mbbootimg/format/mtk_writer_p.h"
#include "mbbootimg/format/sony_elf_writer_p.h"
#include "mbbootimg/header.h"

#define ENSURE_STATE_OR_RETURN(STATES, RETVAL) \
    do { \
        if (!(m_state & (STATES))) { \
            return RETVAL; \
        } \
    } while (0)

#define ENSURE_STATE_OR_RETURN_ERROR(STATES) \
    ENSURE_STATE_OR_RETURN(STATES, WriterError::InvalidState)

/*!
 * \file mbbootimg/writer.h
 * \brief Boot image writer API
 */

/*!
 * \file mbbootimg/writer_p.h
 * \brief Boot image writer private API
 */

/*!
 * \fn FormatWriter::set_option
 *
 * \brief Format writer callback to set option
 *
 * \note This is currently not exposed in the public API. There is no way to
 *       access this function.
 *
 * \param key Option key
 * \param value Option value
 *
 * \return
 *   * Return nothing if the option is handled successfully
 *   * Return WriterError::UnknownOption if the option cannot be handled
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::open
 *
 * \brief Format writer callback to initialize a boot image
 *
 * If this function returns an error code, close() will be called in
 * Writer::open() to clean up any state. Otherwise, close() will be called in
 * Writer::close() when the user closes the Writer.
 *
 * \param file Reference to file handle
 *
 * \return
 *   * Return nothing if no errors occur while initializing the boot image
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::close
 *
 * \brief Format writer callback to finalize and close boot image
 *
 * This function will be called to clean up the state regardless of whether the
 * file is successfully opened. It is guaranteed that this function will only
 * ever be called once after a call to open(). If an error code is returned, the
 * user cannot reattempt the close operation.
 *
 * \param file Reference to file handle
 *
 * \return
 *   * Return nothing if no errors occur while closing the boot image
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::get_header
 *
 * \brief Format writer callback to get Header instance
 *
 * \param file Reference to file handle
 *
 * \return
 *   * Return a Header instance if successful
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::write_header
 *
 * \brief Format writer callback to write header
 *
 * \param file Reference to file handle
 * \param header Header instance to write
 *
 * \return
 *   * Return nothing if the header is successfully written
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::get_entry
 *
 * \brief Format writer callback to get Entry instance
 *
 * \param file Reference to file handle
 *
 * \return
 *   * Return an Entry instance if successful
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::write_entry
 *
 * \brief Format writer callback to write entry
 *
 * \param file Reference to file handle
 * \param entry Entry instance to write
 *
 * \return
 *   * Return nothing if the entry is successfully written
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::write_data
 *
 * \brief Format writer callback to write entry data
 *
 * \note The callback function *must* write \p buf_size bytes or return an error
 *       if it cannot do so.
 *
 * \param file Reference to file handle
 * \param buf Input buffer
 * \param buf_size Size of input buffer
 *
 * \return
 *   * Return number of bytes written if the entry is successfully written
 *   * Return a specific error code if an error occurs
 */

/*!
 * \fn FormatWriter::finish_entry
 *
 * \brief Format writer callback to complete the writing of an entry
 *
 * \param file Reference to file handle
 *
 * \return
 *   * Return nothing if successful
 *   * Return a specific error code if an error occurs
 */

///

namespace mb::bootimg
{

using namespace detail;

FormatWriter::FormatWriter() noexcept = default;

FormatWriter::~FormatWriter() noexcept = default;

oc::result<void> FormatWriter::set_option(const char *key, const char *value)
{
    (void) key;
    (void) value;
    return WriterError::UnknownOption;
}

oc::result<void> FormatWriter::open(File &file)
{
    (void) file;
    return oc::success();
}

oc::result<void> FormatWriter::close(File &file)
{
    (void) file;
    return oc::success();
}

oc::result<void> FormatWriter::finish_entry(File &file)
{
    (void) file;
    return oc::success();
}

/*!
 * \brief Construct new Writer.
 */
Writer::Writer() noexcept
    : m_state(WriterState::New)
    , m_owned_file()
    , m_file()
    , m_format()
{
}

/*!
 * \brief Free a Writer.
 *
 * If the writer has not been closed, it will be closed. Since this is the
 * destructor, it is not possible to get the result of the close operation. To
 * get the result of the close operation, call Writer::close() manually.
 */
Writer::~Writer() noexcept
{
    (void) close();
}

Writer::Writer(Writer &&other) noexcept
    : Writer()
{
    *this = std::move(other);
}

Writer & Writer::operator=(Writer &&rhs) noexcept
{
    if (this != &rhs) {
        (void) close();

        // close() keeps the selected format
        m_format.reset();

        std::swap(m_state, rhs.m_state);
        std::swap(m_owned_file, rhs.m_owned_file);
        std::swap(m_file, rhs.m_file);
        std::swap(m_format, rhs.m_format);
    }

    return *this;
}

/*!
 * \brief Open boot image from filename (MBS).
 *
 * \param filename MBS filename
 *
 * \return Nothing if the boot image is successfully opened. Otherwise, a
 *         specific error code.
 */
oc::result<void> Writer::open_filename(const std::string &filename)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::New);

    auto file = std::make_unique<StandardFile>();
    // Open in read/write mode since some formats need to reread the file
    OUTCOME_TRYV(file->open(filename, FileOpenMode::ReadWriteTrunc));

    return open(std::move(file));
}

/*!
 * \brief Open boot image from filename (WCS).
 *
 * \param filename WCS filename
 *
 * \return Nothing if the boot image is successfully opened. Otherwise, a
 *         specific error code.
 */
oc::result<void> Writer::open_filename_w(const std::wstring &filename)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::New);

    auto file = std::make_unique<StandardFile>();
    // Open in read/write mode since some formats need to reread the file
    OUTCOME_TRYV(file->open(filename, FileOpenMode::ReadWriteTrunc));

    return open(std::move(file));
}

/*!
 * \brief Open boot image from File handle.
 *
 * This function will take ownership of the file handle. When the Writer is
 * closed, the file handle will also be closed.
 *
 * \param file File handle
 *
 * \return Nothing if the boot image is successfully opened. Otherwise, a
 *         specific error code.
 */
oc::result<void> Writer::open(std::unique_ptr<File> file)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::New);

    OUTCOME_TRYV(open(file.get()));

    // Underlying pointer is not invalidated during a move
    m_owned_file = std::move(file);
    return oc::success();
}

/*!
 * \brief Open boot image from File handle.
 *
 * This function will not take ownership of the file handle. When the Writer is
 * closed, the file handle will remain open.
 *
 * \param file File handle
 *
 * \return Nothing if the boot image is successfully opened. Otherwise, a
 *         specific error code.
 */
oc::result<void> Writer::open(File *file)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::New);

    if (!m_format) {
        return WriterError::NoFormatRegistered;
    }

    auto ret = m_format->open(*file);
    if (!ret) {
        (void) m_format->close(*file);
        return ret.as_failure();
    }

    m_state = WriterState::Header;
    m_file = file;

    return oc::success();
}

/*!
 * \brief Close a Writer.
 *
 * This function will close a Writer if it is open. Regardless of the return
 * value, the writer is closed and can no longer be used for further operations.
 *
 * It is important to check the return value of this function instead of relying
 * on the destructor since some formats require steps to finalize the boot image
 * when the writer is closed.
 *
 * \return Nothing if the writer is successfully closed. Otherwise, a specific
 *         error code.
 */
oc::result<void> Writer::close()
{
    auto reset_state = finally([&] {
        m_state = WriterState::New;

        m_owned_file.reset();
        m_file = nullptr;
    });

    oc::result<void> ret = oc::success();

    if (m_state != WriterState::New) {
        ret = m_format->close(*m_file);

        if (m_owned_file) {
            auto close_ret = m_owned_file->close();
            if (ret && !close_ret) {
                ret = std::move(close_ret);
            }
        }
    }

    return ret;
}

/*!
 * \brief Get prepared boot image header instance.
 *
 * Get a prepared Header instance for use with Writer::write_header(). The
 * instance will prevent setting fields not supported by the boot image format.
 *
 * \return Get prepared Header instance if it is successfully fetched.
 *         Otherwise, a specific error code.
 */
oc::result<Header> Writer::get_header()
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::Header);

    // Don't alter state
    return m_format->get_header(*m_file);
}

/*!
 * \brief Write boot image header
 *
 * Write a header to the boot image. It is recommended to use the Header
 * instance provided by Writer::get_header(), but it is not strictly necessary.
 * Fields that are not supported by the boot image format will be silently
 * ignored.
 *
 * \param header Header instance to write
 *
 * \return Nothing if the boot image header is successfully written. Otherwise,
 *         a specific error code.
 */
oc::result<void> Writer::write_header(const Header &header)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::Header);

    OUTCOME_TRYV(m_format->write_header(*m_file, header));

    m_state = WriterState::Entry;
    return oc::success();
}

/*!
 * \brief Get next boot image entry instance.
 *
 * This function will return WriterError::EndOfEntries when there are no more
 * entries to write. It is strongly* recommended to check the return value of
 * Writer::close() when closing the boot image as additional steps for
 * finalizing the boot image could fail.
 *
 * \return The next entry if it is successfully fetched. Otherwise, a specific
 *         error code.
 */
oc::result<Entry> Writer::get_entry()
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::Entry | WriterState::Data);

    // Finish current entry
    if (m_state == WriterState::Data) {
        OUTCOME_TRYV(m_format->finish_entry(*m_file));

        m_state = WriterState::Entry;
    }

    OUTCOME_TRY(entry, m_format->get_entry(*m_file));

    m_state = WriterState::Entry;
    return std::move(entry);
}

/*!
 * \brief Write boot image entry.
 *
 * Write an entry to the boot image. It is *strongly* recommended to use the
 * Entry instance provided by Writer::get_entry(), but it is not strictly
 * necessary. If a different instance of Entry is used, the type field *must*
 * match the type field of the instance returned by Writer::get_entry().
 *
 * \param entry Entry instance to write
 *
 * \return Nothing if the boot image entry is successfully written. Otherwise,
 *         a specific error code.
 */
oc::result<void> Writer::write_entry(const Entry &entry)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::Entry);

    OUTCOME_TRYV(m_format->write_entry(*m_file, entry));

    m_state = WriterState::Data;
    return oc::success();
}

/*!
 * \brief Write boot image entry data.
 *
 * \param[in] buf Input buffer
 * \param[in] size Size of input buffer
 *
 * \return Number of bytes written. If EOF is reached, FileError::UnexpectedEof
 *         will be returned. If any other error occurs, a specific error code
 *         will be returned.
 */
oc::result<size_t> Writer::write_data(const void *buf, size_t size)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::Data);

    // Do not alter state. Stay in WriterState::DATA
    return m_format->write_data(*m_file, buf, size);
}

/*!
 * \brief Get selected boot image format code.
 *
 * \return Boot image format code
 */
std::optional<Format> Writer::format()
{
    if (!m_format) {
        return std::nullopt;
    }

    return m_format->type();
}

static std::unique_ptr<FormatWriter> _construct_format(Format format)
{
    switch (format) {
        case Format::Android:
            return std::make_unique<android::AndroidFormatWriter>(false);
        case Format::Bump:
            return std::make_unique<android::AndroidFormatWriter>(true);
        case Format::Loki:
            return std::make_unique<loki::LokiFormatWriter>();
        case Format::Mtk:
            return std::make_unique<mtk::MtkFormatWriter>();
        case Format::SonyElf:
            return std::make_unique<sonyelf::SonyElfFormatWriter>();
        default:
            MB_UNREACHABLE("Invalid format");
    }
}

/*!
 * \brief Set boot image output format by its code.
 *
 * \param format Boot image format
 *
 * \return Nothing if the format is successfully set. Otherwise, the error code.
 */
oc::result<void> Writer::set_format(Format format)
{
    ENSURE_STATE_OR_RETURN_ERROR(WriterState::New);

    m_format = _construct_format(format);

    return oc::success();
}

/*!
 * \brief Check whether writer is opened
 *
 * \return Whether writer is opened
 */
bool Writer::is_open()
{
    return m_state != WriterState::New;
}

}
