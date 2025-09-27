#include <string>

#include "session.hpp"

namespace fz::ftp {

using namespace std::string_literals;


session::protocol_info::status session::protocol_info::get_status() const
{
	if (!security)
		return protocol_info::insecure;

	if (security->algorithm_warnings)
		return protocol_info::not_completely_secure;

	return protocol_info::secure;
}

std::string session::protocol_info::get_name() const
{
	return security ? "FTPS"s : "FTP"s;
}

}
