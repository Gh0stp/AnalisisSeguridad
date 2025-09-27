#include <libfilezilla/recursive_remove.hpp>
#include <libfilezilla/impersonation.hpp>

#if !defined(FZ_WINDOWS)
#	include "fcntl.h"
#else
#	include "winnetwk.h"
#endif

#include <libfilezilla/impersonation.hpp>
#include "../../strsyserror.hpp"
#include "local_filesys.hpp"
#include "../../strresult.hpp"
#include "../../logger/type.hpp"

namespace fz::tvfs::backends {

namespace {

struct debug
{
	const fz::result &result;

	template <typename String>
	friend String toString(const debug &d)
	{
		using C = typename String::value_type;

		return fz::sprintf(fzS(C, "%d: %s (raw = %d: %s)"), d.result.error_, strresult(d.result), d.result.raw_, strsyserror(d.result.raw_));
	}
};

}

#ifdef FZ_WINDOWS
fz::result local_filesys::add_connection(std::wstring path, const std::wstring &username, const std::wstring &password)
{
	fz::result result;

	NETRESOURCE netResource;
	ZeroMemory(&netResource, sizeof(netResource));
	netResource.dwType = RESOURCETYPE_DISK;
	netResource.lpRemoteName = path.data();

	auto raw = WNetAddConnection2W(&netResource, password.c_str(), username.c_str(), 0);
	switch (raw) {
		case NO_ERROR:
			break;

		case ERROR_ACCESS_DENIED:
		case ERROR_LOGON_FAILURE:
			result = { result::noperm, raw };
			break;

		default:
			result = { result::other, raw };
			break;
	}

	logger_.log(logmsg::debug_debug, L"WNetAddConnection2W(\"%s\", \"****\", \"%s\"): result = %s.", path, username, debug{result});

	return result;
}

void local_filesys::cancel_connection(const std::wstring &path)
{
	auto raw = WNetCancelConnection2W(path.data(), 0, false);
	fz::result result;

	if (raw != NO_ERROR) {
		result = { fz::result::other, raw };
	}

	logger_.log(logmsg::debug_debug, L"WNetCancelConnection2W(\"%s\"): result = %s.", path, debug{result});
}

template <typename T>
fz::result local_filesys::add_connection(const T&, bool &)
{
	return {};
}

fz::result local_filesys::add_connection(const absolute_native_path &path, bool &attempt_made)
{
	// Find the nearest ancestor path in the map, or the path itself.
	for (auto it = creds_.lower_bound(path.str()); it != creds_.end(); ++it) {
		if (fz::starts_with(path.str(), it->first)) {
			// Paths are normalized and never ending with a separator
			bool found
				=  path.str().size() == it->first.size()
				|| path.str()[it->first.size()] == absolute_native_path::separator();

			if (found) {
				attempt_made |= true;
				return add_connection(it->first, it->second.username, it->second.password);
			}
		}
	}

	// No path found, no connection to be made.
	return {};
}

template <typename... Ts>
fz::result local_filesys::add_connections(bool &attempt_made, Ts &&... v)
{
	fz::result res{};

	((res = add_connection(v, attempt_made)) && ...);

	return res;
}

template <typename F, typename... Args>
fz::result local_filesys::invoke(const F & f, Args &&... args)
{
	static auto must_attempt_connection = [](const fz::result &res) {
		if (res.error_ == fz::result::noperm) {
			return true;
		}

		return false;
	};

	fz::result res = std::invoke(f, args...);

	if (must_attempt_connection(res)) {
		bool attempt_made{};

		auto connection_res = add_connections(attempt_made, args...);

		if (attempt_made) {
			if (connection_res) {
				res = std::invoke(f, args...);
			}
			else {
				res = connection_res;
			}
		}
	}

	return res;
}

#	define FZ_LOCAL_FILESYS_INVOKE(func, ...) invoke(func, __VA_ARGS__)
#else
#	define FZ_LOCAL_FILESYS_INVOKE(func, ...) func(__VA_ARGS__)
#endif


local_filesys::local_filesys(logger_interface &logger)
	// This assumes the current username doesn't change during the lifetime of the object.
	: logger_(logger, "local_filesys", { { "filesys user", fz::to_utf8(fz::current_username()) } })
{
	logger_.log(logmsg::debug_debug, L"Constructed");
}

local_filesys::~local_filesys()
{
	logger_.log(logmsg::debug_debug, L"Destroying");
}

void local_filesys::open_file(const absolute_native_path &native_path, file::mode mode, file::creation_flags flags, receiver_handle<open_response> r)
{
	fz::file f;

	auto open = [&f](const auto &... args) {
		return f.open(args...);
	};

	auto res = !native_path
		? result { result::invalid }
		: FZ_LOCAL_FILESYS_INVOKE(open, native_path, mode, flags);

	logger_.log_u(logmsg::debug_debug, L"open_file(%s): fd = %d, result = %s", native_path, std::intptr_t(f.fd()), debug{res});

	return r(res, std::move(f));
}

void local_filesys::open_directory(const absolute_native_path &native_path, receiver_handle<open_response> r)
{
	static auto open = [](const absolute_native_path &native_path, fd_owner &fd) -> fz::result {
	#if defined(FZ_WINDOWS)
		fd = fd_owner(CreateFileW(native_path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
		auto last_error = GetLastError();
	#else
		fd = fd_owner(::open(native_path.c_str(), O_DIRECTORY|O_RDONLY|O_CLOEXEC));
		auto last_error = errno;
	#endif

		if (!fd) {
			switch (last_error) {
				FZ_RESULT_RAW_CASE_WIN(ERROR_ACCESS_DENIED)
				FZ_RESULT_RAW_CASE_WIN(ERROR_LOGON_FAILURE)
				FZ_RESULT_RAW_CASE_NIX(EACCES)
				FZ_RESULT_RAW_CASE_NIX(EPERM)
					return { result::noperm, last_error };

				FZ_RESULT_RAW_CASE_WIN(ERROR_PATH_NOT_FOUND)
				FZ_RESULT_RAW_CASE_WIN(ERROR_FILE_NOT_FOUND)
				FZ_RESULT_RAW_CASE_NIX(ENOENT)
				FZ_RESULT_RAW_CASE_NIX(ENOTDIR)
					return { result::nodir, last_error };

				default:
					return { result::other, last_error };
			}
		}

		return { result::ok };
	};

	fd_owner fd;
	result res;

	if (!native_path)
		res = {result::invalid};
	else {
		res = FZ_LOCAL_FILESYS_INVOKE(open, native_path, fd);
	}

	logger_.log_u(logmsg::debug_debug, L"open_directory(%s): fd = %d, result = %s", native_path, std::intptr_t(fd.get()), debug{res});

	return r(res, std::move(fd));
}

void local_filesys::rename(const absolute_native_path &path_from, const absolute_native_path &path_to, receiver_handle<rename_response> r)
{
	result res;

	if (!path_from || !path_to)
		res = {result::invalid};
	else
		res = FZ_LOCAL_FILESYS_INVOKE(fz::rename_file, path_from, path_to, true);

	logger_.log_u(logmsg::debug_debug, L"rename(%s, %s): result = %s", path_from, path_to, debug{res});

	return r(res);
}

void local_filesys::remove_file(const absolute_native_path &path, receiver_handle<remove_response> r)
{
	result res;

	static const auto remove = [](const absolute_native_path &path) -> fz::result {
		return fz::remove_file(path, true);
	};

	if (!path)
		res = { result::invalid };
	else
		res = FZ_LOCAL_FILESYS_INVOKE(remove, path);

	logger_.log_u(logmsg::debug_debug, L"remove_file(%s): result = %s", path, debug{res});

	return r(res);
}

void local_filesys::remove_directory(const absolute_native_path &path, bool recursive, receiver_handle<remove_response> r)
{
	result res;

	static const auto remove = [](const absolute_native_path &path, bool recursive) -> fz::result {
		if (recursive) {
			if (fz::recursive_remove().remove(path)) {
				return { result::ok };
			}

			return { result::other };
		}

		return fz::remove_dir(path, true);
	};

	if (!path) {
		res = { result::invalid };
	}
	else {
		res = FZ_LOCAL_FILESYS_INVOKE(remove, path, recursive);
	}

	logger_.log_u(logmsg::debug_debug, L"remove_directory(%s): result = %s", path, debug{res});

	return r(res);
}

void local_filesys::info(const absolute_native_path &path, bool follow_links, receiver_handle<info_response> r)
{
	result res;

	bool is_link;
	fz::local_filesys::type type = fz::local_filesys::unknown;
	int64_t size;
	datetime modification_time;
	int mode;

	auto get_file_info = [&](const absolute_native_path &path, bool follow_links) {
		return fz::local_filesys::get_file_info(path, is_link, type, &size, &modification_time, &mode, follow_links);
	};

	if (!path)
		res = { result::invalid };
	else
		res = FZ_LOCAL_FILESYS_INVOKE(get_file_info, path, follow_links);

	logger_.log_u(logmsg::debug_debug, L"info(%s): is_link = %d, type = %d, size = %d,  result = %s", path, is_link, type, size, debug{res});

	return r(res, is_link, type, size, modification_time, mode);
}

void local_filesys::mkdir(const absolute_native_path &path, bool recurse, mkdir_permissions permissions, receiver_handle<mkdir_response> r)
{
	result res;
	native_string last_created;

	if (!path)
		res = { result::invalid };
	else
		res = FZ_LOCAL_FILESYS_INVOKE(fz::mkdir, path, recurse, permissions, &last_created);

	if (res && last_created.empty()) {
		res = { result::preexisting };
	}

	logger_.log_u(logmsg::debug_debug, L"mkdir(%s): result = %s", path, debug{res});

	return r(res);
}

void local_filesys::set_mtime(const absolute_native_path &path, const datetime &mtime, receiver_handle<set_mtime_response> r)
{
	result res;

	if (!path) {
		res = { result::invalid };
	}
	else {
		res = FZ_LOCAL_FILESYS_INVOKE(fz::local_filesys::set_modification_time, path, mtime);
	}

	logger_.log_u(logmsg::debug_debug, L"set_mtime(%s): result = %s", path, debug{res});

	return r(res);
}

void local_filesys::set_credentials(const credentials_map &creds [[maybe_unused]], receiver_handle<set_credentials_response> r)
{
	result res;

#ifdef FZ_WINDOWS
	credentials_map creds_to_cancel;

	// Cancel all connections potentially already made that differ from the new ones.
	std::set_difference(std::make_move_iterator(creds_.begin()), std::make_move_iterator(creds_.end()), creds.begin(), creds.end(), std::inserter(creds_to_cancel, creds_to_cancel.end()));
	for (const auto &c: creds_to_cancel) {
		cancel_connection(c.first);
	}
	creds_to_cancel.clear();

	creds_ = creds;
#endif

	logger_.log_u(logmsg::debug_debug, L"set_credentials(): result = %s", debug{res});

	return r(res);
}

}
