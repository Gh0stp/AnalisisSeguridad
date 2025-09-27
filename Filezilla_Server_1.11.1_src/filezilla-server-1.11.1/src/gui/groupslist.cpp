#include <libfilezilla/glue/wx.hpp>

#include "groupslist.hpp"

#include "locale.hpp"

GroupsList::GroupsList(wxWindow *parent)
	: MappedItemListManager(parent, "New Group", _S("Available groups"))
{
}
