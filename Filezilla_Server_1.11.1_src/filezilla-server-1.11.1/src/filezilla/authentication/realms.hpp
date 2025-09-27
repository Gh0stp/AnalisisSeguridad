#ifndef FZ_AUTHENTICATION_REALMS_HPP
#define FZ_AUTHENTICATION_REALMS_HPP

#include <string>

#include "../enum_names.hpp"
#include "../serialization/types/common.hpp"
#include "../serialization/types/containers.hpp"

namespace fz::authentication
{

struct realm
{
	enum status: std::uint8_t {
		deferred,
		enabled,
		disabled
	};

	FZ_ENUM_NAMES_FRIEND_DEFINE_FOR(deferred, enabled, disabled)

	std::string name;
	enum status status{deferred};

	template <typename Archive>
	void serialize(Archive &ar) {
		using namespace fz::serialization;

		ar.attribute(name, "name").attribute(status, "status");
	}
};

}

#endif // FZ_AUTHENTICATION_REALMS_HPP
