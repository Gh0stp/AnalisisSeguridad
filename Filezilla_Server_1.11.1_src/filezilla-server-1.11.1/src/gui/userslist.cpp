#include <libfilezilla/glue/wx.hpp>

#include "userslist.hpp"

#include "locale.hpp"

UsersList::UsersList(wxWindow *parent)
	: MappedItemListManager(parent, "New User", _S("Available users"))
{
}
