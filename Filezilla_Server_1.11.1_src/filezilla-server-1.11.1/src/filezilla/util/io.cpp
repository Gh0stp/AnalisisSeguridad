#include <memory>
#include <cerrno>

#include <libfilezilla/buffer.hpp>
#include <libfilezilla/local_filesys.hpp>

#include "io.hpp"
#include "filesystem.hpp"
#include "../logger/file.hpp"
#include "../strresult.hpp"

namespace fz::util::io {

rwresult write(file &file, const void *data, std::size_t size)
{
	if (!file.opened()) {
		return rwresult{rwresult::invalid, FZ_RESULT_RAW_INVALID_HANDLE};
	}

	std::string_view view(reinterpret_cast<const char*>(data), size);

	while (view.size()) {
		auto res = file.write2(view.data(), view.size());
		if (!res) {
			return res;
		}

		view.remove_prefix(res.value_);
	}

	return {};
}

rwresult write(file &file, const buffer &b)
{
	return write(file, b.get(), b.size());
}

rwresult read(file &file, void *data, std::size_t size)
{
	if (!file.opened()) {
		return rwresult{rwresult::invalid, FZ_RESULT_RAW_INVALID_HANDLE};
	}

	auto orig_size = size;
	rwresult res;

	while (size > 0) {
		res = file.read2(data, size);

		if (!res) {
			return res;
		}

		if (res.value_ == 0) {
			break;
		}

		size -= res.value_;
		data = reinterpret_cast<char *>(data) + res.value_;
	}

	return rwresult(orig_size - size);
}

rwresult read(file &file, buffer &b)
{
	static constexpr std::size_t chunk_size = 128*1024;

	rwresult res;

	while (true) {
		res = read(file, b.get(chunk_size), chunk_size);

		if (!res || res.value_ == 0) {
			break;
		}

		b.add(res.value_);
	}

	return res;
}

buffer read(file &file, rwresult *result)
{
	buffer b;

	auto res = read(file, b);

	if (result) {
		*result = res;
	}

	return b;
}

rwresult copy(file &in, file &out)
{
	static constexpr std::size_t chunk_size = 128*1024;

	fz::buffer b;
	rwresult res;

	while (true) {
		res = read(in, b.get(chunk_size), chunk_size);

		if (!res || !res.value_) {
			break;
		}

		b.add(res.value_);

		res = write(out, b);
		if (!res) {
			break;
		}

		b.consume(res.value_);
	}

	return {};
}

rwresult copy(file &&in, file &&out)
{
	fz::file own_in = std::move(in);
	fz::file own_out = std::move(out);

	return copy(own_in, own_out);
}

rwresult copy(native_string_view in, native_string_view out, file::creation_flags flags)
{
	fz::file in_file(native_string(in), fz::file::mode::reading);
	fz::file out_file(native_string(out), fz::file::mode::writing, flags | file::creation_flags::empty);

	return copy(in_file, out_file);
}

result copy_dir(native_string_view in, native_string_view out, mkdir_permissions permissions)
{
	result fsres = fz::mkdir(native_string(out), true, permissions);
	if (!fsres) {
		return fsres;
	}

	local_filesys in_dir;
	in_dir.begin_find_files(native_string(in), false, false);

	native_string name;
	bool is_link;
	local_filesys::type type;

	auto rwres2fsres = [](rwresult res) -> result {
		switch (res.value_) {
			case rwresult::none:       return {result::none};
			case rwresult::invalid:    return {result::invalid, res.raw_};
			case rwresult::nospace:    return {result::nospace, res.raw_};
			case rwresult::wouldblock: return {result::other, res.raw_ };
			default:                   return {result::other, res.raw_ };
		}
	};

	while (in_dir.get_next_file(name, is_link, type, nullptr, nullptr, nullptr)) {
		auto src = fs::native_path(in) / name;
		auto dst = fs::native_path(out) / name;

		if (type == local_filesys::dir) {
			auto res = copy_dir(src, dst, permissions);
			if (fsres) {
				fsres = res;
			}
		}
		else
		if (type == local_filesys::file) {
			auto cflags = file::creation_flags::empty;

			if (permissions == mkdir_permissions::cur_user)
				cflags = cflags | file::creation_flags::current_user_only;
			else
			if (permissions == mkdir_permissions::cur_user_and_admins)
				cflags = cflags | file::creation_flags::current_user_and_admins_only;

			auto rwres = io::copy(src, dst, cflags);
			if (fsres) {
				fsres = rwres2fsres(rwres);
			}
		}
	}

	return fsres;
}

}
