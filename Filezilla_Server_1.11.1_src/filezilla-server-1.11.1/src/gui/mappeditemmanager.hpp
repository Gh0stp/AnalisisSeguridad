#ifndef MAPPEDITEMMANAGER_HPP
#define MAPPEDITEMMANAGER_HPP

#include <wx/simplebook.h>

#include "nameditemlist.hpp"
#include "helpers.hpp"

template <typename Map, typename ItemEditor>
class MappedItemManager: public wxPanel, virtual protected MappedItemTraits<Map>
{
	using key_type = typename MappedItemTraits<Map>::key_type;
	using value_type = typename MappedItemTraits<Map>::value_type;

	class List: public MappedItemListManager<Map>
	{
	public:
		List(MappedItemManager *owner, const key_type &default_item_name, const wxString &title)
			: MappedItemListManager<Map>(owner, default_item_name, title)
			, owner_(owner)
		{
		}

	protected:
		bool less_than_item_comparator(value_type &lhs, value_type &rhs) override
		{
			return owner_->less_than_item_comparator(lhs, rhs);
		}

		wxListItemAttr *get_item_attr(value_type &v) override
		{
			return owner_->get_item_attr(v);
		}

		bool is_name_allowed(const key_type &n) override
		{
			return owner_->is_name_allowed(n);
		}

		key_type canonicalize_name(const key_type &n) override
		{
			return owner_->canonicalize_name(n);
		}

	private:
		MappedItemManager *owner_;
	};

public:
	MappedItemManager(wxWindow *parent, const key_type &default_item_name, const wxString &list_title, const wxString &item_kind)
		: wxPanel(parent)
	{
		wxHBox(this, 0) = {
			wxSizerFlags(0).Expand() >>= list_ = new List(this, default_item_name, list_title),
			wxSizerFlags(1).Expand() >>= book_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(this) | [&](auto *p) {
				editor_ = wxPage<ItemEditor>(p, wxS("*Enabled*"));

				wxPage(p, wxS("*Disabled*"), true) |[&](auto *p) {
					wxVBox(p) = {
						wxEmptySpace,
						wxSizerFlags(0).Align(wxALIGN_CENTER) >>= wxLabel(p, _F("Select or add a %s in the list to the side.", item_kind)),
						wxEmptySpace
					};
				};
			}
		};

		list_->Bind(List::Event::AboutToDeselectItem, [this](auto &ev){
			if (!Validate() || !TransferDataFromWindow())
				ev.Veto();
			else
				ev.Skip();
		});

		list_->Bind(List::Event::SelectedItem, [this](auto &ev){
			ev.Skip();

			TransferDataToWindow();
		});

		if constexpr (std::is_invocable_v<decltype(&ItemEditor::SetItem), ItemEditor&, typename Map::value_type*>) {
			list_->Bind(List::Event::Changed, [this](auto &){
				auto item = list_->GetSelectedItem();
				if (item) {
					editor_->SetItem(item);
				}
			});
		}
	}

	void SetMap(Map &map)
	{
		list_->SetMap(map);
	}

	ItemEditor *GetItemEditor()
	{
		return editor_;
	}

	bool TransferDataToWindow() override
	{
		if (!wxPanel::TransferDataToWindow())
			return false;

		auto item = list_->GetSelectedItem();

		if (item) {
			book_->ChangeSelection(0);
			if constexpr (std::is_invocable_v<decltype(&ItemEditor::SetItem), ItemEditor&, typename Map::value_type*>) {
				editor_->SetItem(item);
			}
			else
			if constexpr (std::is_invocable_v<decltype(&ItemEditor::SetItem), ItemEditor&, typename Map::mapped_type*>) {
				editor_->SetItem(&item->second);
			}
			else {
				static_assert(fz::util::delayed_assert<ItemEditor>, "ItemEditor has no suitable SetItem method");
			}
		}
		else {
			book_->ChangeSelection(1);
			editor_->SetItem(nullptr);
		}

		return true;
	}

private:
	wxSimplebook *book_{};
	ItemEditor *editor_{};
	List *list_{};
};

#endif // MAPPEDITEMMANAGER_HPP
