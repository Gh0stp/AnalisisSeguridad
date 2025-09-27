#include "file_based_authenticator.hpp"

namespace fz::authentication {

namespace {
	using xml_archiver = util::xml_archiver<authentication::file_based_authenticator::groups, authentication::file_based_authenticator::users>;
}

serialization::xml_input_archive::error_t file_based_authenticator::load_into(fz::authentication::file_based_authenticator::groups &groups, fz::authentication::file_based_authenticator::users &users)
{
	if (xml_archiver *a = static_cast<xml_archiver *>(xml_archiver_.get())) {
		return a->load_into(groups, users);
	}

	return { EINVAL, "file_based_authenticator was constructed without paths to the users and groups files." };
}

bool file_based_authenticator::save(const native_string &groups_path, const groups &groups, const native_string &users_path, const users &users)
{
	return xml_archiver::save_now(
		{groups, { "", groups_path }},
		{users, { "", users_path }}
	) == 0;
}

}
