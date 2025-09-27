#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/format.hpp>

#include <wx/listctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/timer.h>

#include "fluidcolumnlayoutmanager.hpp"
#include "helpers.hpp"
#include "../filezilla/util/parser.hpp"

#include "nameditemlist.hpp"

#include "locale.hpp"
#include "glue.hpp"
#include "listctrlex.hpp"

template <typename Char>
class BasicNamedItemListManager<Char>::ListCtrl: public wxListCtrlEx
{
	static int constexpr timer_ms = 10;

	const String default_item_name_;
	const wxString title_;

	std::unique_ptr<BasicNamedItemList<Char>> items_;
	long selected_item_ = -1;
	long selecting_item_ = -1;

public:
	ListCtrl(BasicNamedItemListManager<Char> *parent, const String &default_item_name_, const wxString &title)
		: wxListCtrlEx(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
					   wxTAB_TRAVERSAL | wxLC_VIRTUAL | wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_SINGLE_SEL)
		, default_item_name_(default_item_name_)
		, title_(title)
	{
		auto cl = new FluidColumnLayoutManager(this);

		cl->SetColumnWeight(InsertColumn(0, title_), 1);

		Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, [&](wxListEvent &ev) {
			auto named_list = GetList();

			if (!named_list || !named_list->IsNameAllowed(named_list->GetName(ev.GetIndex()))) {
				ev.Veto();
			}
			else {
				ev.Skip();
			}
		});

		Bind(wxEVT_LIST_END_LABEL_EDIT, [&](wxListEvent &ev) {
			ev.Skip();

			if (!items_) {
				return ev.Veto();
			}

			auto name = [&text = ev.GetItem().GetText()] {
				if constexpr (std::is_same_v<char, char_type>) {
					return fz::to_utf8(text);
				}
				else
				if constexpr (std::is_same_v<wchar_t, char_type>) {
					return fz::to_wstring(text);
				}
			}();

			fz::trim(name);

			if (name.empty()) {
				return ev.Veto();
			}

			name = items_->CanonicalizeName(name);

			if (name.empty()) {
				return ev.Veto();
			}

			if (ev.GetIndex() < 0 || static_cast<size_t>(ev.GetIndex()) >= items_->GetSize()) {
				return ev.Veto();
			}

			if (items_->Has(name)) {
				return ev.Veto();
			}

			auto old_name = items_->GetName(ev.GetItem());
			if (old_name.empty()) {
				return ev.Veto();
			}

			if (!BasicNamedItemListManager::Event::Changing.Process(this, GetParent(), old_name, name).IsAllowed()) {
				ev.Veto();
				return;
			}

			if (!items_->Rename(ev.GetItem(), name)) {
				return ev.Veto();
			}

			SetItemState(ev.GetIndex(), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			BasicNamedItemListManager::Event::Changed.Process(this, GetParent(), old_name, name);
		});

		Bind(Event::ItemSelecting, [&](Event &ev) {
			selecting_item_ = ev.GetItem();

			if (ev.GetItem() != selected_item_ && !CanDeselectItem())
				ev.Veto();
		});

		Bind(wxEVT_LIST_ITEM_SELECTED, [&](wxListEvent &ev) {
			ev.Skip();
			OnItemSelected(ev.GetIndex());
		});
	}

	void SetList(std::unique_ptr<BasicNamedItemList<Char>> items)
	{
		selected_item_ = -1;
		items_ = std::move(items);

		if (items_) {
			SetItemCount((long)items_->GetSize());

			if (GetItemCount() > 0) {
				SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
				selected_item_ = 0;
			}

		}
		else {
			SetItemCount(0);
		}

		Refresh();
		Layout();

		BasicNamedItemListManager::Event::SelectedItem.Process(this, GetParent());
	}

	BasicNamedItemList<Char> *GetList()
	{
		return items_.get();
	}

	long GetSelectedItem() const {
		return selected_item_;
	}

	bool Append(long item_to_copy = -1)
	{
		if (!items_ || !CanDeselectItem())
			return false;

		auto new_name = item_to_copy >= 0 ? items_->GetName(item_to_copy) : default_item_name_;
		if (new_name.empty()) {
			return false;
		}

		while (items_->Has(new_name)) {
			static const auto get_name_and_index = [](StringView name) {
				std::size_t idx = 0;

				// Try to parse the index number between parens
				fz::util::parseable_range r(name.crbegin(), name.crend());
				if (lit(r, ')') && until_lit(r, '(')) {
					auto open_paren_it = (++r.it).base();

					// This should encompass the number we're looking for
					fz::util::parseable_range n(std::next(open_paren_it), std::prev(name.cend()));
					if (parse_int(n, idx) && eol(n)) {
						auto new_name = name.substr(0, std::size_t(open_paren_it-name.cbegin()));
						if (!new_name.empty()) {
							if (new_name.back() == ' ')
								new_name.remove_suffix(1);

							name = new_name;
						}
						else
							idx = 0;
					}
				}

				return std::pair{name, idx};
			};

			auto [name, idx] = get_name_and_index(new_name);

			new_name = fz::sprintf(fzS(char_type, "%s (%zu)"), name, ++idx);
		}

		if (!BasicNamedItemListManager::Event::Changing.Process(this, GetParent(), String(), new_name).IsAllowed()) {
			return false;
		}

		if (!items_->Append(new_name, item_to_copy)) {
			return false;
		}

		SetItemCount((long)items_->GetSize());

		SetItemState(GetItemCount()-1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		RefreshItem(GetItemCount()-1);

		BasicNamedItemListManager::Event::Changed.Process(this, GetParent(), String(), new_name);

		EditLabel(GetItemCount()-1);

		return true;
	}

	void RenameSelected()
	{
		if (selected_item_ < 0) {
			return;
		}

		EditLabel(selected_item_);
	}

	void RemoveSelected()
	{
		if (!items_ || selected_item_ < 0) {
			return;
		}

		auto old_name = items_->GetName(selected_item_);
		if (old_name.empty()) {
			return;
		}

		if (!BasicNamedItemListManager::Event::Changing.Process(this, GetParent(), old_name, String()).IsAllowed()) {
			return;
		}

		auto item_to_remove = selected_item_;

		if (selected_item_ == GetItemCount()-1) {
			selected_item_ -= 1;
		}
		else {
			selected_item_ += 1;
		}

		SetItemState(item_to_remove, ~wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

		if (selected_item_ >= 0) {
			SetItemState(selected_item_, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}

		auto removed = items_->Remove(item_to_remove);
		wxASSERT_MSG(removed, _S("Couldn't remove the item from the list."));

		SetItemCount((long)items_->GetSize());

		if (item_to_remove < selected_item_) {
			selected_item_ -= 1;
		}

		if (selected_item_ >= 0) {
			SetItemState(selected_item_, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}

		BasicNamedItemListManager::Event::SelectedItem.Process(this, GetParent());
		BasicNamedItemListManager::Event::Changed.Process(this, GetParent(), old_name, String());

		Refresh();

	}

	bool CanDeselectItem()
	{
		if (selected_item_ < 0) {
			return true;
		}

		bool allowed = BasicNamedItemListManager::Event::AboutToDeselectItem.Process(this, GetParent()).IsAllowed();

		return allowed;
	}

	size_t size() const
	{
		return items_ ? items_->GetSize() : 0;
	}

private:
	void OnItemSelected(long item)
	{
		if (item == selected_item_) {
			return;
		}

		if (selecting_item_ != item) {
			if (!CanDeselectItem()) {
				SetItemState(selected_item_, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
				return;
			}
		}

		selecting_item_ = -1;
		selected_item_ = item;

		BasicNamedItemListManager::Event::SelectedItem.Process(this, GetParent());
	}

	wxString OnGetItemText(long item, long column) const override
	{
		wxASSERT(items_ != nullptr);
		wxCHECK(0 <= item && item <= (long)items_->GetSize(), wxEmptyString);
		wxCHECK(column == 0, wxEmptyString);

		return fz::to_wxString(items_->GetName(item));
	}

	wxListItemAttr *OnGetItemAttr(long item) const override
	{
		wxASSERT(items_ != nullptr);
		wxCHECK(0 <= item && item <= (long)items_->GetSize(), nullptr);

		return items_->GetAttr(item);
	}
};

template <typename Char>
BasicNamedItemListManager<Char>::BasicNamedItemListManager(wxWindow *parent, const String &default_item_name, const wxString &title)
	: wxPanel(parent)
{
	wxVBox(this, 0) = {
		wxSizerFlags(1).Expand() >>= list_ = new ListCtrl(this, default_item_name, title),

		wxGBox(this, 2, {0, 1}) = {
			add_button_ = new wxButton(this, wxID_ANY, _S("A&dd")),
			remove_button_ = new wxButton(this, wxID_ANY, _S("Re&move")),

			duplicate_button_ = new wxButton(this, wxID_ANY, _S("D&uplicate")),
			rename_button_ = new wxButton(this, wxID_ANY, _S("Re&name")),
		}
	};

	Bind(Event::SelectedItem, [&](Event &ev){
		ev.Skip();

		update_buttons();
	});

	list_->Bind(wxEVT_LIST_BEGIN_LABEL_EDIT, [&](wxListEvent &ev) {
		ev.Skip();

		update_buttons();
	});

	list_->Bind(wxEVT_LIST_END_LABEL_EDIT, [&](wxListEvent &ev) {
		ev.Skip();

		update_buttons();
	});

	add_button_->Bind(wxEVT_BUTTON, [&](auto) {
		if (!list_->Append()) {
			return;
		}

		update_buttons();
	});

	remove_button_->Bind(wxEVT_BUTTON, [&](auto) {
		list_->EndEditLabel(true);
		list_->RemoveSelected();

		update_buttons();
	});

	duplicate_button_->Bind(wxEVT_BUTTON, [&](auto){
		if (!list_->Append(list_->GetSelectedItem()))
			return;

		update_buttons();
	});

	rename_button_->Bind(wxEVT_BUTTON, [&](auto){
		list_->RenameSelected();
	});
}

template <typename Char>
void BasicNamedItemListManager<Char>::SetList(std::unique_ptr<BasicNamedItemList<Char>> list)
{
	list_->SetList(std::move(list));
}

template <typename Char>
BasicNamedItemList<Char> *BasicNamedItemListManager<Char>::GetList()
{
	return list_->GetList();
}

template <typename Char>
long BasicNamedItemListManager<Char>::GetSelectedItem() const
{
	return list_->GetSelectedItem();
}

template <typename Char>
size_t BasicNamedItemListManager<Char>::size() const
{
	return list_->size();
}

template <typename Char>
void BasicNamedItemListManager<Char>::update_buttons()
{
	CallAfter([&] {
		bool add{}, remove{}, duplicate{}, rename{};
		auto named_list = GetList();

		if (named_list) {
			add = remove = duplicate = rename = true;

			if (list_->GetEditControl()) {
				rename = false;
			}
			else {
				auto selected = GetSelectedItem();
				if (selected < 0 || !named_list->IsNameAllowed(named_list->GetName(selected))) {
					rename = remove = duplicate = false;
				}
			}
		}

		add_button_->Enable(add);
		remove_button_->Enable(remove);
		duplicate_button_->Enable(duplicate);
		rename_button_->Enable(rename);
	});
}

template class BasicNamedItemList<char>;
template class BasicNamedItemListManager<char>;
template class BasicNamedItemList<wchar_t>;
template class BasicNamedItemListManager<wchar_t>;
