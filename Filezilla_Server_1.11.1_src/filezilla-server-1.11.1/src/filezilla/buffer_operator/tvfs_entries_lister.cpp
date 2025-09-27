#include "tvfs_entries_lister.hpp"

namespace fz::buffer_operator
{

tvfs_entry_filter::~tvfs_entry_filter()
{}

bool tvfs_entry_filter::operator ()(const tvfs::entry &) const
{
	return true;
}

const tvfs_entry_filter &fz::buffer_operator::tvfs_entry_filter::none()
{
	const static tvfs_entry_filter filter;

	return filter;
}

}
