#include <map>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

#include "xml_upgrader.hpp"

#include "../filezilla/preprocessor/cat.hpp"
#include "../filezilla/util/scope_guard.hpp"
#include "../filezilla/util/filesystem.hpp"
#include "../filezilla/util/tools.hpp"


namespace {

using upgrader_function = std::function<xml_upgrader::error_t(pugi::xml_node root, fz::build_info::version_info fz_from_version_, fz::build_info::version_info fz_to_version_, fz::logger::modularized &logger)>;
using upgrader_version2function = std::map<fz::build_info::version_info, upgrader_function>;
using upgrader_element2version2function = std::unordered_map<std::string_view, upgrader_version2function>;

struct upgrader_element_creator
{
	upgrader_element_creator(upgrader_element2version2function &e2v2f, std::string_view element, std::string_view type, std::size_t line)
		: element(element)
		, type(type)
		, line(line)
		, e2v2f_(e2v2f)
	{
	}

	upgrader_element_creator(const upgrader_element_creator &) = delete;
	upgrader_element_creator(upgrader_element_creator &&) = delete;

	template <typename F>
	std::enable_if_t<std::is_invocable_v<F, upgrader_element_creator&, upgrader_version2function&>, bool>
	operator<<(F && f) &&
	{
		auto [it, created] = e2v2f_.try_emplace(element);
		if (!created) {
			std::fprintf(stderr, "*** XML UPGRADER: fatal error at %s:%zu: %s(\"%s\") has already been created.\n", __FILE__, line, std::string(type).c_str(), std::string(element).c_str());
			std::abort();
		}

		f(*this, it->second);
		return true;
	}

	const std::string_view element;
	const std::string_view type;
	const std::size_t line;

private:
	upgrader_element2version2function &e2v2f_;
};

struct upgrader_function_creator
{
	upgrader_function_creator(upgrader_version2function &v2f, fz::build_info::version_info version, upgrader_element_creator &ec, std::size_t line)
		: v2f_(v2f)
		, version_(version)
		, ec_(ec)
		, line_(line)
	{
	}

	upgrader_function_creator(const upgrader_function_creator &) = delete;
	upgrader_function_creator(upgrader_function_creator &&) = delete;

	template <typename F>
	void operator<<(F && f) &&
	{
		if (version_ > fz::build_info::version) {
			std::fprintf(stderr, "*** XML UPGRADER: fatal error at %s:%zu: version in FZ_UPGRADE_TO(\"%s\") cannot be greater than current fz::build_info::version %s.\n", __FILE__, line_, std::string(version_).c_str(), std::string(fz::build_info::version).c_str());
			std::abort();
		}

		if (version_.dev && version_ != fz::build_info::version) {
			std::fprintf(stderr, "*** XML UPGRADER: fatal error at %s:%zu: you must replace the version in FZ_UPGRADE_TO(\"%s\") with the actual release version.\n", __FILE__, line_, std::string(version_).c_str());
			std::abort();
		}

		auto [it, created] = v2f_.try_emplace(version_);
		if (!created) {
			std::fprintf(stderr, "*** XML UPGRADER: fatal error at %s:%zu: FZ_UPGRADE_TO(\"%s\") has already been created in %s(\"%s\").\n", __FILE__, line_, std::string(version_).c_str(), std::string(ec_.type).c_str(), std::string(ec_.element).c_str());
			std::abort();
		}

		it->second = std::forward<F>(f);
	}

private:
	upgrader_version2function &v2f_;
	fz::build_info::version_info version_;
	upgrader_element_creator &ec_;
	std::size_t line_;
};

}

static xml_upgrader::error_t upgrade(upgrader_element2version2function &e2v2f, std::string_view element, fz::build_info::version_info from_version, fz::build_info::version_info to_version, std::string_view node_name, pugi::xml_node node, fz::logger::modularized &logger)
{
	logger.log(fz::logmsg::debug_debug, L"Invoked upgrade(element = \"%s\", node = \"%s\", from_version = %s, to_version = %s)", element, node.name(), from_version, to_version);

	if (!node) {
		return {};
	}

	auto v2f_it = e2v2f.find(element);
	if (v2f_it == e2v2f.end()) {
		logger.log(fz::logmsg::error, L"Could not find element \"%s\"", element);
		return {};
	}

	auto old_version = logger.find_meta("version");
	auto old_upgrading_to = logger.find_meta("upgrading to");
	logger.erase_meta("version");
	logger.erase_meta("upgrading to");

	fz::util::scope_guard reinsert_meta = [&] {
		if (old_version) {
			logger.insert_meta("version", *old_version);
		}

		if (old_upgrading_to) {
			logger.insert_meta("upgrading to", *old_upgrading_to);
		}
	};

	fz::logger::modularized modlogger(logger, std::string(node_name));
	auto f_it = v2f_it->second.upper_bound(from_version);

	if (modlogger.should_log(fz::logmsg::debug_debug)) {
		if (f_it  == v2f_it->second.end()) {
			modlogger.log(fz::logmsg::debug_debug, L"Element \"%s\" is already at the desired version.", element);
		}
	}

	for (; f_it != v2f_it->second.end() && f_it->first <= to_version; ++f_it) {
		modlogger.insert_meta("version", from_version);
		modlogger.insert_meta("upgrading to", f_it->first);

		modlogger.log(fz::logmsg::debug_debug, L"Invoking upgrader for root node \"%s\".", node.name());

		if (auto err = f_it->second(node, from_version, f_it->first, modlogger)) {
			return err;
		}
	}

	return {};
}

[[maybe_unused]] static std::pair<std::string_view, pugi::xml_node> get_node(const char *name, pugi::xml_node root)
{
	return {name, root.child(name)};
}

[[maybe_unused]] static std::pair<std::string_view, pugi::xml_node>  get_node(pugi::xml_node node, pugi::xml_node)
{
	return {"", node};
}

static upgrader_element2version2function nodes_upgrader;
static upgrader_element2version2function files_upgrader;

#define FZ_UPGRADE_NODE(node_id) \
	[[maybe_unused]] static const auto FZ_PP_CAT(FZ_PP_CAT(node_upgrader_,__LINE__),_) = upgrader_element_creator(nodes_upgrader, node_id, "FZ_UPGRADE_NODE", __LINE__) << []([[maybe_unused]] upgrader_element_creator &ec, [[maybe_unused]] upgrader_version2function &v2f) \
/***/

#define FZ_UPGRADE_FILE(file_id) \
	[[maybe_unused]] static const auto FZ_PP_CAT(FZ_PP_CAT(file_upgrader_,__LINE__),_) = upgrader_element_creator(files_upgrader, file_id, "FZ_UPGRADE_FILE", __LINE__) << []([[maybe_unused]] upgrader_element_creator &ec, [[maybe_unused]] upgrader_version2function &v2f) \
/***/

#define FZ_UPGRADE_TO(version) \
	upgrader_function_creator(v2f, version, ec, __LINE__) << [](pugi::xml_node root, fz::build_info::version_info fz_from_version_ [[maybe_unused]], fz::build_info::version_info fz_to_version_ [[maybe_unused]], fz::logger::modularized &logger [[maybe_unused]]) -> xml_upgrader::error_t \
/***/

#define FZ_UPGRADE(name, node) \
	do { \
		auto [fz_node_name_, fz_node_] = get_node(node, root); \
		if (auto err = upgrade(nodes_upgrader, name, fz_from_version_, fz_to_version_, fz_node_name_, fz_node_, logger)) { \
			return err;	\
		} \
	} while(false) \
/***/

#define FZ_UPGRADE_ALL(name, node) \
	do { \
		for (auto [fz_node_name_, fz_node_] = get_node(node, root); fz_node_; fz_node_ = fz_node_.next_sibling(node)) { \
			if (auto err = upgrade(nodes_upgrader, name, fz_from_version_, fz_to_version_, fz_node_name_, fz_node_, logger)) { \
					return err;	\
			} \
		} \
	} while(false) \
/***/

xml_upgrader::xml_upgrader(fz::logger_interface &logger)
	: logger_(logger, "XML Upgrader")
{
	if (logger.should_log(fz::logmsg::debug_debug)) {
		logger_.log(fz::logmsg::debug_debug, L"Registered file upgraders:");
		for (auto &f: files_upgrader) {
			logger_.log(fz::logmsg::debug_debug, L"  - %s", f.first);
			for (auto &v: f.second) {
				logger_.log(fz::logmsg::debug_debug, L"    - %s", v.first);
			}
		}

		logger_.log(fz::logmsg::debug_debug, L"Registered node upgraders:");
		for (auto &f: nodes_upgrader) {
			logger_.log(fz::logmsg::debug_debug, L"  - %s", f.first);
			for (auto &v: f.second) {
				logger_.log(fz::logmsg::debug_debug, L"    - %s", v.first);
			}
		}
	}
}

xml_upgrader::worker::worker(xml_upgrader &xu, std::string fileid)
	: logger_(xu.logger_)
	, fileid_(std::move(fileid))
{}


xml_upgrader::error_t xml_upgrader::worker::operator ()(fz::native_string_view filename, pugi::xml_node root, fz::build_info::version_info &product_version)
{
	auto err = upgrade(files_upgrader, fileid_, product_version, fz::build_info::version, fz::to_string(filename), root, logger_);
	if (!err) {
		product_version = fz::build_info::version;
	}

	return err;
}

/*************************************************************************************************************************
 *                                   DEFINE THE UPGRADING BLOCKS BELOW THIS COMMENT
 *************************************************************************************************************************/

FZ_UPGRADE_FILE("settings.xml") {
#ifdef FZ_WINDOWS
	FZ_UPGRADE_TO("1.10.1") {
		FZ_UPGRADE("logger::file::options", "logger");

		return {};
	};
#endif
};

FZ_UPGRADE_FILE("imported configuration file") {
#ifdef FZ_WINDOWS
	FZ_UPGRADE_TO("1.10.1") {
		FZ_UPGRADE("logger::file::options", "logger_options");

		return {};
	};
#endif
};

#ifdef FZ_WINDOWS
FZ_UPGRADE_NODE("logger::file::options") {

	// Fix for bug introduced in 1.10.0.
	FZ_UPGRADE_TO("1.10.1") {
		if (auto name = root.child("name")) {
			fz::util::fs::windows_path path(name.text().as_string());

			if (path.base().str_view() == "service-log.txt") {
				path.make_parent();

				auto nstmp = path.base().str();
				if (fz::starts_with(nstmp, "ns") && fz::ends_with(nstmp, ".tmp")) {
					for (const auto &f: fz::util::fs::native_path(fz::to_native_from_utf8(path))) {
						if (fz::starts_with(f.base().str_view(), L"service-log")) {
							fz::remove_file(f, false);
						}
					}

					fz::util::fs::windows_path instpath = fz::to_utf8(fz::util::get_own_executable_directory());

					name.text().set((instpath / "Logs" / "filezilla-server.log").c_str());
					auto include_headers = root.child("include_headers");
					if (!include_headers) {
						include_headers = root.append_child("include_headers");
					}

					include_headers.text().set(true);

					logger.log(fz::logmsg::status, L"Logger configuration was wrong. File path reset to %s.", name.text().as_string());
				}
			}
		}

		return {};
	};
};
#endif
