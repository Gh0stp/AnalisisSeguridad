#ifndef WXFZFTPUSERSLIST_HPP
#define WXFZFTPUSERSLIST_HPP

#include "../filezilla/authentication/file_based_authenticator.hpp"
#include "nameditemlist.hpp"

class UsersList: public MappedItemListManager<fz::authentication::file_based_authenticator::users>
{
	using users = map_type;

public:
	UsersList(wxWindow *parent);
};

#endif // WXFZFTPUSERSLIST_HPP
