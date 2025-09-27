#ifndef FZ_AUTHENTICATION_FILE_BASED_AUTHENTICATOR_HPP
#define FZ_AUTHENTICATION_FILE_BASED_AUTHENTICATOR_HPP

#include <unordered_map>

#include <libfilezilla/hash.hpp>
#include <libfilezilla/impersonation.hpp>

#include "../logger/modularized.hpp"
#include "../authentication/authenticator.hpp"
#include "../serialization/types/containers.hpp"
#include "../serialization/types/variant.hpp"
#include "../serialization/types/optional.hpp"
#include "../serialization/types/binary_address_list.hpp"
#include "../serialization/types/fs_path.hpp"
#include "../serialization/types/tvfs.hpp"
#include "../util/traits.hpp"
#include "../util/xml_archiver.hpp"
#include "../util/integral_ops.hpp"
#include "../tvfs/limits.hpp"
#include "../util/copies_counter.hpp"
#include "realms.hpp"
#include "credentials.hpp"

#include "../tcp/binary_address_list.hpp"

namespace fz::authentication {

class file_based_authenticator: public authenticator {
public:
	struct rate_limits {
		rate::type inbound{rate::unlimited};
		rate::type outbound{rate::unlimited};
		rate::type session_inbound{rate::unlimited};
		rate::type session_outbound{rate::unlimited};

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;

			ar.optional_attribute(with_unlimited{inbound, rate::unlimited}, "inbound")
			  .optional_attribute(with_unlimited{outbound, rate::unlimited}, "outbound")
			  .optional_attribute(with_unlimited{session_inbound, rate::unlimited}, "session_inbound")
			  .optional_attribute(with_unlimited{session_outbound, rate::unlimited}, "session_outbound");
		}
	};

	struct auth_entry {
		tvfs::mount_table mount_table{};
		struct rate_limits rate_limits{};
		tvfs::open_limits session_open_limits{};
		tcp::binary_address_list allowed_ips{};
		tcp::binary_address_list disallowed_ips{};
		std::uint16_t session_count_limit{};

		std::string description{};
		std::vector<realm> realms{};

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;

			ar(
				nvp(mount_table, "", "mount_point"),
				optional_nvp(rate_limits, "rate_limits"),
				optional_nvp(allowed_ips, "allowed_ips"),
				optional_nvp(disallowed_ips, "disallowed_ips"),
				optional_nvp(session_open_limits, "session_open_limits"),
				optional_nvp(with_unlimited{session_count_limit, 0}, "session_count_limit"),
				optional_nvp(description, "description"),
				optional_nvp(realms, "", "realm")
			);
		}
	};

	struct group_entry: public auth_entry {
		using auth_entry::auth_entry;

		//TODO: implement C3 linearization
		//std::vector<std::string> groups;
	};

	struct user_entry: public auth_entry {
		using auth_entry::auth_entry;

		bool enabled{true};
		std::vector<std::string> groups{};
		authentication::credentials credentials;
		available_methods methods { credentials.get_most_secure_methods() };

		template <typename Archive>
		void serialize(Archive &ar)
		{
			using namespace fz::serialization;

			ar.optional_attribute(enabled, "enabled")(
				nvp(static_cast<auth_entry &>(*this), ""),
				nvp(groups, "", "group"),
				nvp(credentials, ""),
				optional_nvp(methods, "methods")
			);
		}
	};

	struct groups: std::unordered_map<std::string /*name*/, group_entry> {
		using unordered_map::unordered_map;

		static inline const std::string invalid_chars_in_name = "<>\r\n\0";

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;
			ar(nvp(with_key_name(*this, "name"), "", "group"));
		}
	};

	struct users: users_map<user_entry> {
		using users_map<user_entry>::users_map;

		static inline const std::string system_user_name = "<system user>";
		static inline const std::string invalid_chars_in_name = "<>\r\n\0";

		struct impersonator
		{
			struct msw
			{
				bool enabled{false};

				native_string name;
				native_string password;

				template <typename Archive>
				void serialize(Archive &ar) {
					using namespace fz::serialization;
					ar.optional_attribute(enabled, "enabled")(
						nvp(name, "name"),
						nvp(password, "password")
					);
				}

				impersonation_token get_token() const;
			};

			struct nix
			{
				bool enabled{false};

				native_string name;
				native_string group;

				template <typename Archive>
				void serialize(Archive &ar) {
					using namespace fz::serialization;
					ar.optional_attribute(enabled, "enabled")(
						nvp(name, "name"),
						nvp(group, "group")
					);
				}

				impersonation_token get_token() const;
			};

		#ifdef FZ_WINDOWS
			using native = msw;
		#else
			using native = nix;
		#endif

			struct any: std::variant<msw, nix>
			{
				using variant::variant;

				impersonator::msw *msw();
				impersonator::nix *nix();
				impersonator::native *native();

				impersonation_token get_token() const;
			};
		};

		impersonator::any default_impersonator = impersonator::native();

		// If users defer to groups and groups defer to system policies, these are the policies that apply.
		// In case these realms' status is `deferred', then the default policy is to enable the realm.
		std::vector<realm> realms{};

		tvfs::backend::credentials_map mount_credentials;

		template <typename Archive>
		void serialize(Archive &ar) {
			using namespace fz::serialization;
			ar(
				optional_nvp(default_impersonator, "default_impersonator"),
				optional_nvp(realms, "", "realm"),
				optional_nvp(with_key_name(mount_credentials, "native_path"), "", "mount_credentials"),
				nvp(with_key_name(*this, "name"), "", "user")
			);
		}
	};

	file_based_authenticator(thread_pool &thread_pool, event_loop &event_loop, logger_interface &logger, rate_limit_manager &rlm, native_string impersonator_exe = {});
	file_based_authenticator(thread_pool &thread_pool, event_loop &event_loop, logger_interface &logger, rate_limit_manager &rlm, native_string groups_path, native_string users_path, native_string impersonator_exe = {});
	file_based_authenticator(thread_pool &thread_pool, event_loop &event_loop, logger_interface &logger, rate_limit_manager &rlm, native_string groups_path, groups &&groups, native_string users_path, users &&users, native_string impersonator_exe = {});
	~file_based_authenticator() override;

	void set_groups_and_users(groups &&groups, users &&users);
	void get_groups_and_users(groups &groups, users &users);

	// Creates a temporary user with random username and password.
	// This user will not be saved on file or serialized in any other way
	std::pair<std::string /*name*/, std::string /*password*/> make_temp_user(tvfs::mount_table mt);
	bool remove_temp_user(const std::string &name);

	bool load();
	bool save(util::xml_archiver_base::event_dispatch_mode mode = util::xml_archiver_base::delayed_dispatch);

	void set_save_result_event_handler(fz::event_handler *handler);

	serialization::xml_input_archive::error_t load_into(fz::authentication::file_based_authenticator::groups &groups, fz::authentication::file_based_authenticator::users &users);

	static bool save(const native_string &groups_path, const groups &groups, const native_string &users_path, const users &users);

	void authenticate(std::string_view realm, std::string_view name, const methods_list &methods, address_type family, std::string_view ip, event_handler &target, logger::modularized::meta_map meta_for_logging = {}) override;
	void stop_ongoing_authentications(event_handler &target) override;


private:
	struct group_limiters {
		std::shared_ptr<rate_limiter> shared_rate_limiter;
		std::shared_ptr<util::limited_copies_counter> session_count_limiter;
	};

	static void sanitize(groups &groups, users &users, logger_interface *logger = nullptr);

	void update_shared_user(user &user, const user_entry &entry, logger::modularized &logger_);
	void update_group_limiters(group_limiters &limiters, const groups::value_type &g);
	std::shared_ptr<tvfs::backend> make_shared_tvfs_backend(impersonation_token &token, logger::modularized &logger_);
	shared_user get_or_make_shared_user(const std::string &name, const user_entry &entry, bool is_from_system, impersonation_token &&token, logger::modularized &logger_);
	group_limiters &get_or_make_group_limiters(const groups::value_type &g);
	void save_later();
	void update();

	mutable fz::mutex mutex_{true};

	thread_pool &thread_pool_;
	event_loop &event_loop_;
	logger::modularized logger_;
	rate_limit_manager &rlm_;
	std::unordered_map<event_handler *, async_handler> async_handlers_;

	class worker;
	using workers = std::list<worker>;
	std::unique_ptr<workers> workers_{};

	groups groups_{};
	users users_{};
	users temp_users_{};

	std::unordered_map<std::string, group_limiters> group_limiters_;
	users_map<weak_user> weak_users_map_;

	native_string impersonator_exe_;

	std::unique_ptr<util::xml_archiver_base> xml_archiver_;
};

}

FZ_SERIALIZATION_SPECIALIZE_ALL(fz::authentication::file_based_authenticator::groups, unversioned_member_serialize)
FZ_SERIALIZATION_SPECIALIZE_ALL(fz::authentication::file_based_authenticator::users, unversioned_member_serialize)

#endif // FZ_AUTHENTICATION_FILE_BASED_AUTHENTICATOR_HPP
