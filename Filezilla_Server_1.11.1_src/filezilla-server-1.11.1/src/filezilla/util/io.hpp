#ifndef FZ_UTIL_IO_HPP
#define FZ_UTIL_IO_HPP

#include <limits>

#include <libfilezilla/file.hpp>
#include <libfilezilla/buffer.hpp>
#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/fsresult.hpp>

#include "traits.hpp"

#include "../strresult.hpp"

namespace fz::util::io {

//! Writes the full size of the data pointed to by the \param data to \param file
//! \note in case of error, no assumption must be made about how much data was effectively written.
rwresult write(fz::file &file, const void *data, std::size_t size);

//! Writes the full content of the container \param c to \param file
//! \returns true in case of success, false otherwise
template <typename Container, std::enable_if_t<is_contiguous_v<Container> && std::is_trivially_copyable_v<typename Container::value_type>>* = nullptr>
rwresult write(fz::file &file, const Container &c)
{
	return write(file, c.data(), c.size() * sizeof(typename Container::value_type));
}

//! Writes the full content of the buffer \param b to \param file
//! \returns true in case of success, false otherwise
rwresult write(fz::file &file, const buffer &b);

//! Writes the full content of the value \param v to \param file, if it's trivially copyable.
//! \returns true in case of success, false otherwise
template <typename Value, std::enable_if_t<!is_contiguous_v<Value> && std::is_trivially_copyable_v<Value>>* = nullptr>
rwresult write(fz::file &file, const Value &v)
{
	return write(file, &v, sizeof(v));
}

//! Writes the full content of the array \param v to \param file, if the elements of the array are trivially copyable.
//! \returns true in case of success, false otherwise
template <typename Char, std::size_t N, std::enable_if_t<!is_contiguous_v<Char> && std::is_trivially_copyable_v<Char>>* = nullptr>
rwresult write(fz::file &file, const Char (&v)[N])
{
	return write(file, &v, sizeof(Char)*(N-1));
}

//! Takes ownership of \param file and forwards the call to one of the other \ref write_all functions.
template <typename... Args>
auto write(fz::file &&file, Args &&... args) -> decltype(write(file, std::forward<Args>(args)...))
{
	fz::file f = std::move(file);

	return write(f, std::forward<Args>(args)...);
}

//! Opens the file for writing, truncating it if it exists already, and forwards the call to one of the other \ref write_all functions.
template <typename... Args>
auto write(native_string_view file_name, Args &&... args) -> decltype(write(std::declval<file>(), std::forward<Args>(args)...))
{
	fz::file f(native_string(file_name), file::mode::writing, file::empty);

	return write(f, std::forward<Args>(args)...);
}

//! Reads up to size bytes into the buffer pointed to by the \param data
//! \note in case of error, no assumption must be made about how much data was effectively read.
rwresult read(fz::file &file, void *data, std::size_t size);

//! Reads the value \param v from \param file, if it's trivially copyable.
//! \returns true in case of success, false otherwise
template <typename Value, std::enable_if_t<std::is_trivially_copyable_v<Value>>* = nullptr>
rwresult read(fz::file &file, Value &v)
{
	rwresult res = read(file, &v, sizeof(v));
	if (res && res.value_ != sizeof(v)) {
		// Premature EOF
		return rwresult{rwresult::other, FZ_RESULT_RAW_EOF};
	}
	return res;
}

//! Reads the content of the file and appends it to the the buffer
//! \note in case of error, no assumption must be made about how much data was effectively read.
rwresult read(fz::file &file, fz::buffer &b);

//! Reads the content of the file into a buffer
//! \returns the buffer.
//! \note in case of error, no assumption must be made about how much data was effectively read.
buffer read(file &file, rwresult *res = nullptr);

//! Takes ownership of \param file and forwards the call to one of the other \ref read functions.
template <typename... Args>
auto read(fz::file &&file, Args &&... args) -> decltype(read(file, std::forward<Args>(args)...))
{
	fz::file f = std::move(file);

	return read(f, std::forward<Args>(args)...);
}

//! Opens the file for reading and forwards the call to one of the other \ref read functions.
template <typename... Args>
auto read(native_string_view file_name, Args &&... args) -> decltype(read(std::declval<file>(), std::forward<Args>(args)...))
{
	fz::file f(native_string(file_name), file::mode::reading);

	return read(f, std::forward<Args>(args)...);
}

//! Copies the content of the file \param in onto the file \param out
rwresult copy(fz::file &in, fz::file &out);

//! Copies the content of the file \param in onto the file \param out
//! Takes ownership of the files
rwresult copy(fz::file &&in, fz::file &&out);

//! Copies the content of the file \param in onto the file \param out
//! Takes ownership of the files
rwresult copy(native_string_view in, native_string_view out, fz::file::creation_flags flags = fz::file::creation_flags::empty);

//! Copies the content of the directory \param in onto the directory \param out, recursively.
//! Destination is created if it doesn't exist yet, with the passed in flags.
result copy_dir(native_string_view in, native_string_view out, mkdir_permissions permissions = mkdir_permissions::normal);

}
#endif // FZ_UTIL_IO_HPP
