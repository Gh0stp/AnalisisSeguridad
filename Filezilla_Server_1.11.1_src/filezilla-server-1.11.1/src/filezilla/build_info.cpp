#include "build_info.hpp"
#include "util/parser.hpp"

#ifdef HAVE_CONFIG_H
#	include "config.hpp"
#endif

#ifndef FZ_BUILD_TYPE
#	define FZ_BUILD_TYPE personal
#endif

#ifndef FZ_BUILD_FLAVOUR
#	define FZ_BUILD_FLAVOUR standard
#endif

#ifndef PACKAGE_VERSION
#	define PACKAGE_VERSION "0.0.0"
#endif

#ifndef FZ_BUILD_HOST
#	define FZ_BUILD_HOST "unknown";
#endif

#include "build_info_gen.hpp"

#ifndef FZ_COMMIT_ID
#	define FZ_COMMIT_ID "unknown source"
#endif

#ifndef FZ_BUILD_IS_DIRTY
#	define FZ_BUILD_IS_DIRTY 0
#endif

namespace fz::build_info {

const build_type type = build_type::FZ_BUILD_TYPE;
const flavour_type flavour = flavour_type::FZ_BUILD_FLAVOUR;
const version_info version = PACKAGE_VERSION;
const std::string package_name = PACKAGE_NAME;
const std::string host = FZ_BUILD_HOST;
const std::string url = PACKAGE_URL;
const std::string copyright = PACKAGE_COPYRIGHT;
const std::string commit_id = FZ_COMMIT_ID;
const bool is_dirty = FZ_BUILD_IS_DIRTY;

static_assert(version_info(PACKAGE_VERSION), "PACKAGE_VERSION must be defined to a valid version, but instead it's [" PACKAGE_VERSION "]");

const std::string warning_message = []() constexpr {
	if constexpr (type != build_type::nightly)
		return "";
	else {
		return
			"Attention, you are using a Nightly Build, built automatically every night.\n"
			"Please be advised that nightly builds are untested, only use them if you absolutely need to test changes that were made since the last stable release.\n"
			"Use FileZilla Server nightly builds at your own risk. This build might not work and may damage your data.";
	}
}();

bool convert(std::string_view s, flavour_type &f)
{
	f = {};

	// First check if the string is in a valid format: must be a C-like identifier.
	if (util::parseable_range r(s); !(lit(r, 'a', 'z') || lit(r, 'A', 'Z') || lit(r, '_')))
		return false;
	else
	while (!eol(r)) {
		if (!(lit(r, 'a', 'z') || lit(r, 'A', 'Z') || lit(r, '_') || lit(r, '0', '9')))
			return false;
	}

	if (s == "standard")
		f = flavour_type::standard;
	else
	if (s == "professional_enterprise")
		f = flavour_type::professional_enterprise;

	return true;
}

const std::string date = [] {
#ifdef FZ_BUID_DATE
	return FZ_BUILD_DATE;
#else
	static constexpr std::string_view date = __DATE__;
	static constexpr auto month = []() constexpr {
		constexpr const std::string_view months[12] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Ago", "Sep", "Oct", "Nov", "Dec"
		};

		auto m = date.substr(0, 3);

		for (int i = 0; i < 12; ++i) {
			if (m == months[i])
				return i + 1;
		}

		return 0;
	}();

	static constexpr char date_array[10] = {
		date[7], date[8], date[9], date[10],    // year
		'-',
		month/10 + '0', month%10 + '0',         // month
		'-',
		(date[4] == ' ' ? '0' : date[4]), date[5] // day
	};

	return std::string{date_array, sizeof(date_array)};
#endif
}();

#if FZ_BUILD_IS_DIRTY
#	define FZ_BUILD_DIRTY_ ", dirty"
#else
#	define FZ_BUILD_DIRTY_ ""
#endif

const std::string full_version_string = [] {
	std::string ret = PACKAGE_NAME " " PACKAGE_VERSION " (";
	ret += date;
	ret += ", " FZ_BUILD_HOST;
	ret += ", " FZ_COMMIT_ID FZ_BUILD_DIRTY_")";

	return ret;
}();

}
