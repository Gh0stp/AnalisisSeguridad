#include <libfilezilla/glue/wx.hpp>

#include <algorithm>

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/dcclient.h>
#include <wx/renderer.h>

#include "rearrangedlist.hpp"
#include "helpers.hpp"
#include "locale.hpp"

RearrangedList::RearrangedList(wxWindow *parent, const wxString &main_column_name, const std::vector<wxString> extra_column_names)
	: wxPanel(parent)
{
	auto flags = wxDV_HORIZ_RULES | wxDV_VERT_RULES | wxDV_SINGLE;

	if (main_column_name.empty() && std::all_of(extra_column_names.begin(), extra_column_names.end(), [](auto &n) { return n.empty();})) {
		flags |= wxDV_NO_HEADER;
	}

	wxHBox(this, 0) =	{
		wxSizerFlags(1).Expand() >>= list_ = new wxDataViewListCtrl(this, nullID, wxDefaultPosition, wxDefaultSize, flags),
		wxSizerFlags(0).Border(wxLEFT, 0) >>= wxVBox(this) = {
			up_ = new wxButton(this, nullID, _S("&Up")),
			down_ = new wxButton(this, nullID, _S("&Down"))
		}
	};

	list_->AppendToggleColumn(wxEmptyString, wxDATAVIEW_CELL_ACTIVATABLE, wxCOL_WIDTH_AUTOSIZE, wxALIGN_LEFT, 0);
	list_->AppendTextColumn(main_column_name, wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE, wxALIGN_LEFT, 0);

	for (auto &n: extra_column_names) {
		list_->AppendTextColumn(n, wxDATAVIEW_CELL_EDITABLE, wxCOL_WIDTH_AUTOSIZE, wxALIGN_LEFT, 0);
	}

	update_buttons();

	list_->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [&](auto) {
		update_buttons();
	});

	list_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [&](wxDataViewEvent &ev) {
		wxDataViewItem item = ev.GetItem();
		if (item.IsOk() && ev.GetColumn() > 1) {
			list_->EditItem(item, ev.GetDataViewColumn());
		}
	});

	up_->Bind(wxEVT_BUTTON, [&](auto) {
		move_selected_row(-1);
	});

	down_->Bind(wxEVT_BUTTON, [&](auto) {
		move_selected_row(1);
	});

	// This is just a heuristic: we really have no way to get the row height of the wxDataViewListCtrl.
	std::tie(row_height_, header_height_) = [&] {
		wxClientDC dc(list_);

		auto checkbox_height = wxRendererNative::Get().GetCheckBoxSize(list_).GetHeight();
		auto text_height = dc.GetTextExtent(wxT("TEST")).GetHeight();

	#ifdef __WXMAC__
		int padding = 1;
	#elif __WXMSW__
		int padding = 13;
	#else
		int padding = 8;
	#endif

		return std::pair(std::max(checkbox_height, text_height) + padding, text_height + padding);
	}();

	if (flags & wxDV_NO_HEADER) {
		header_height_ = 0;
	}
}

bool RearrangedList::swap_rows(int row1, int row2)
{
	if (row1 < 0 || row2 < 0 || row1 >= list_->GetItemCount() || row2 >= list_->GetItemCount()) {
		return false;
	}

	for (unsigned int c=0; c < list_->GetColumnCount(); ++c) {
		wxVariant value1;
		wxVariant value2;

		list_->GetValue(value1, (unsigned)row1, c);
		list_->GetValue(value2, (unsigned)row2, c);

		list_->SetValue(value2, (unsigned)row1, c);
		list_->SetValue(value1, (unsigned)row2, c);
	}

	return true;
}

void RearrangedList::move_selected_row(int dir)
{
	if (dir == 0) {
		return;
	}

	auto selected = list_->GetSelectedRow();
	auto target = selected + (dir > 0 ? +1 : -1);

	if (swap_rows(selected, target)) {
		list_->SelectRow((unsigned)target);
		update_buttons();
	}
}

void RearrangedList::update_buttons()
{
	auto selected = list_->GetSelectedRow();
	list_->SetFocus();
	list_->SetFocusFromKbd();
	up_->Enable(selected > 0);
	down_->Enable(selected >= 0 && selected < list_->GetItemCount()-1);
}

void RearrangedList::append_item(bool active, std::initializer_list<wxString> values)
{
	wxVector<wxVariant> item(list_->GetColumnCount());
	item[0] = active;

	std::size_t i = 1;

	for (auto &e: values) {
		if (i < item.size()) {
			item[i++] = e;
			continue;
		}

		break;
	}

	for (; i < item.size(); ++i) {
		item[i] = wxEmptyString;
	}

	list_->AppendItem(item);
}

void RearrangedList::rearrange()
{
	auto available_end = available_.end();
	auto discarded_size = discarded_.size();

	auto is_available = [&](const std::string &item) {
		auto it = std::find(available_.begin(), available_end, item);
		if (it != available_end) {
			// Let's get this one out of the way, while keeping the relative order of all the remaining availables.
			std::rotate(it, it+1, available_end);
			available_end -= 1;
			return true;
		}

		return false;
	};

	auto discard = [&](unsigned i) {
		auto &discarded_item = discarded_.emplace_back();
		discarded_item.reserve(list_->GetColumnCount()-1);
		for (unsigned c = 1; c < list_->GetColumnCount(); ++c) {
			discarded_item.push_back(list_->GetTextValue(i, c));
		}
		list_->DeleteItem(i);
	};

	auto undiscard = [&](unsigned i) {
		wxVector<wxVariant> item(list_->GetColumnCount());
		item[0] = false;

		auto to_undiscard_it = discarded_.begin() + i;
		for (unsigned c = 1; c < item.size(); ++c) {
			item[c] = (*to_undiscard_it)[c-1];
		}
		discarded_.erase(to_undiscard_it);
		discarded_size -=1;

		list_->AppendItem(item);
	};

	// First go through the current items and discard all those that are not available anymore.
	for (int i=0; i < list_->GetItemCount();) {
		auto our = fz::to_utf8(list_->GetTextValue((unsigned)i, 1));
		if (!is_available(our)) {
			discard((unsigned)i);
		}
		else {
			++i;
		}
	}

	// Then go through the previously discarded ones and restore those that are available again.
	for (std::size_t i = 0; i < discarded_size; ++i) {
		auto our = fz::to_utf8(discarded_[i][0]);
		if (is_available(our)) {
			undiscard((unsigned)i);
		}
		else {
			++i;
		}
	}

	// Finally, add to the current items all the remaning available ones, deactivated.
	for (auto it = available_.begin(); it != available_end; ++it) {
		append_item(false, {fz::to_wxString(*it)});
	}

	if (row_height_ > 0) {
		best_client_size_ = DoGetBestSize() - GetWindowBorderSize();
		auto total_items_height = header_height_ + list_->GetItemCount() * row_height_;

		if (total_items_height > best_client_size_.GetHeight()) {
			best_client_size_.SetHeight(total_items_height);
		}
	}
}

void RearrangedList::SetAvailableItems(std::vector<std::string> items)
{
	std::sort(items.begin(), items.end());
	items.erase(std::unique(items.begin(), items.end()), items.end());

	available_ = std::move(items);

	rearrange();
}

void RearrangedList::ClearActiveItems()
{
	clear();
	rearrange();
}

wxSize RearrangedList::DoGetBestClientSize() const
{
	return best_client_size_;
}

void RearrangedList::clear()
{
	list_->DeleteAllItems();
	discarded_.clear();
}
