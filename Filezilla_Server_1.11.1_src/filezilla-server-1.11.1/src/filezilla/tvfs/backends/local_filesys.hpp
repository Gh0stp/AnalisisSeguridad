#ifndef FZ_TVFS_BACKENDS_LOCAL_FILESYS_HPP
#define FZ_TVFS_BACKENDS_LOCAL_FILESYS_HPP

#include <unordered_set>
#include "../../logger/modularized.hpp"
#include "../backend.hpp"

namespace fz::tvfs::backends {


class local_filesys final: public backend
{
public:

	local_filesys(logger_interface &logger);
	~local_filesys() override;

	void open_file(const absolute_native_path &native_path, file::mode mode, file::creation_flags flags, receiver_handle<open_response> r) override;
	void open_directory(const absolute_native_path &native_path, receiver_handle<open_response> r) override;
	void rename(const absolute_native_path &path_from, const absolute_native_path &path_to, receiver_handle<rename_response> r) override;
	void remove_file(const absolute_native_path &path, receiver_handle<remove_response> r) override;
	void remove_directory(const absolute_native_path &path, bool recursive, receiver_handle<remove_response> r) override;
	void info(const absolute_native_path &path, bool follow_links, receiver_handle<info_response> r) override;
	void mkdir(const absolute_native_path &path, bool recurse, mkdir_permissions permissions, receiver_handle<mkdir_response> r) override;
	void set_mtime(const absolute_native_path &path, const datetime &mtime, receiver_handle<set_mtime_response> r) override;
	void set_credentials(const credentials_map &creds, receiver_handle<set_credentials_response> r) override;

private:
	logger::modularized logger_;

#ifdef FZ_WINDOWS
	credentials_map creds_;

	template <typename F, typename... Args>
	fz::result invoke(const F & f, Args &&... args);

	template <typename... Ts>
	fz::result add_connections(bool &attempt_made, Ts &&... v);

	template <typename T>
	fz::result add_connection(const T&, bool &attempt_made);

	fz::result add_connection(const absolute_native_path &path, bool &attempt_made);

	fz::result add_connection(std::wstring path, const std::wstring &username, const std::wstring &password);

	void cancel_connection(const std::wstring &path);
#endif
};

}
#endif // FZ_TVFS_BACKENDS_LOCAL_FILESYS_HPP
