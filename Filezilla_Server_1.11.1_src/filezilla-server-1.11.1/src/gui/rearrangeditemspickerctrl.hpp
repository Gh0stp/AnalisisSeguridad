#ifndef WXREARRANGEDITEMSPICKERCTRL_HPP
#define WXREARRANGEDITEMSPICKERCTRL_HPP

#include <vector>
#include <string>

#include <wx/panel.h>
#include <wx/combo.h>

#include "rearrangedlist.hpp"

class RearrangedItemsPickerCtrl: public wxPanel
{
public:
	RearrangedItemsPickerCtrl(wxWindow *parent, wxString main_column_name = {}, std::vector<wxString> extra_column_names = {});

	void SetAvailableItems(std::vector<std::string> available);

	template <typename T>
	auto SetActiveItems(const T &items) -> decltype(static_cast<RearrangedList*>(std::declval<wxComboCtrl>().GetPopupControl()->GetControl())->SetActiveItems(items))
	{
		auto list = static_cast<RearrangedList*>(combo_->GetPopupControl()->GetControl());

		wxASSERT_MSG(list != nullptr, "The list has not been created.");

		list->SetActiveItems(items);
		combo_->SetText(combo_->GetPopupControl()->GetStringValue());
	}

	template <typename T>
	auto GetActiveItems(T &items) -> decltype(static_cast<RearrangedList*>(std::declval<wxComboCtrl>().GetPopupControl()->GetControl())->GetActiveItems(items))
	{
		auto list = static_cast<RearrangedList*>(combo_->GetPopupControl()->GetControl());

		wxASSERT_MSG(list != nullptr, "The list has not been created.");

		list->GetActiveItems(items);
	}

	void ClearActiveItems();

private:

	class Popup;

	wxComboCtrl *combo_;
};

#endif // WXREARRANGEDITEMSPICKERCTRL_HPP
