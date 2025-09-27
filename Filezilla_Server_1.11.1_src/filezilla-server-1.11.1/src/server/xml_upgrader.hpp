#ifndef XML_UPGRADER_HPP
#define XML_UPGRADER_HPP

#include "../filezilla/logger/modularized.hpp"
#include "../filezilla/serialization/archives/xml.hpp"

class xml_upgrader
{
	fz::logger::modularized logger_;

public:
	using error_t = fz::serialization::xml_input_archive::error_t;

private:
	struct worker
	{
		worker(xml_upgrader &xu, std::string fileid);
		error_t operator()(fz::native_string_view filename, pugi::xml_node root, fz::build_info::version_info &product_version);

	private:
		fz::logger::modularized logger_;
		std::string fileid_;
	};

public:
	xml_upgrader(fz::logger_interface &logger);

	worker settings{*this, "settings.xml"};
	worker imported_configuration_file{*this, "imported configuration file"};
};

#endif // XML_UPGRADER_HPP
