#ifndef WXFZFTPGROUPSLIST_HPP
#define WXFZFTPGROUPSLIST_HPP

#include "../filezilla/authentication/file_based_authenticator.hpp"
#include "nameditemlist.hpp"

class GroupsList: public MappedItemListManager<fz::authentication::file_based_authenticator::groups>
{
	using groups = map_type;

public:
	GroupsList(wxWindow *parent);
};

#endif // WXFZFTPGROUPSLIST_HPP
