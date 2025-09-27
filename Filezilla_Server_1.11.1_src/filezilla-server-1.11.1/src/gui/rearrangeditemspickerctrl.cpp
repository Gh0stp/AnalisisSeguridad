#include <libfilezilla/glue/wx.hpp>

#include <wx/combo.h>
#include <wx/sizer.h>
#include <wx/rearrangectrl.h>
#include <wx/log.h>
#include <wx/statline.h>
#include <wx/dataview.h>

#include "rearrangeditemspickerctrl.hpp"
#include "rearrangedlist.hpp"
#include "helpers.hpp"

#if defined(__WXOSX__)
#	define FZ_REARRANGEDITEMPICKER_USE_DIALOG 0
#else
#   define FZ_REARRANGEDITEMPICKER_USE_DIALOG 0
#endif

class RearrangedItemsPickerCtrl::Popup: public wxComboPopup {
public:
	Popup(wxString main_column_name = {}, std::vector<wxString> extra_column_names = {})
		: main_column_name_(std::move(main_column_name))
		, extra_column_names_(std::move(extra_column_names))
	{}

	RearrangedList *GetList() const
	{
		return list_;
	}

	bool LazyCreate() override
	{
		return false;
	}

	bool Create(wxWindow *parent) override
	{
		list_ = new RearrangedList(parent, main_column_name_, extra_column_names_);

		list_->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, [&](wxCommandEvent &ev) {
			ev.Skip();
			m_combo->CallAfter([&]{m_combo->SetText(GetStringValue());});
		});

		return true;
	}

	wxWindow *GetControl() override
	{
		return list_;
	}

	wxString GetStringValue() const override
	{
		wxASSERT_MSG(list_ != nullptr, "The list has not been created.");

		std::vector<std::string> active;
		list_->GetActiveItems(active);

		wxString value;

		for (auto &a: active) {
			if (!value.empty())
				value += wxS(" ＞ ");

			value += fz::to_wxString(a);
		}

		return value;
	}

	wxSize GetAdjustedSize(int min_width, int pref_height, int max_height) override
	{
		wxASSERT_MSG(list_ != nullptr, "The list has not been created.");

		return {min_width, std::min((pref_height + max_height)/2, list_->GetBestSize().GetHeight())};
	}

private:
	wxString main_column_name_;
	std::vector<wxString> extra_column_names_;
	RearrangedList *list_{};
};

RearrangedItemsPickerCtrl::RearrangedItemsPickerCtrl(wxWindow *parent, wxString main_column_name, std::vector<wxString> extra_column_names)
	: wxPanel(parent)
{
#if FZ_REARRANGEDITEMPICKER_USE_DIALOG
	struct DiagCombo: wxComboCtrl
	{
		DiagCombo(RearrangedItemsPickerCtrl *owner)
			: wxComboCtrl(owner, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCC_STD_BUTTON | wxCB_READONLY)
		{
		}

	private:
		wxDialog *dlg_{};

		void OnMove(wxMoveEvent &ev)
		{
			dlg_->SetPosition(ClientToScreen(wxPoint(GetPosition().x, GetPosition().y + GetSize().GetHeight())));
			ev.Skip();
		}

		void OnSize(wxSizeEvent &ev)
		{
			dlg_->SetSize(wxSize(GetSize().GetWidth(), dlg_->GetSize().GetHeight()));
			ev.Skip();
		}

		void OnButtonClick() override
		{
			Disable();

			wxPushDialog(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP).DoNotForceCenter() | [this](auto &dlg) {
				dlg_ = &dlg;

				auto list = GetPopupControl()->GetControl();
				auto old_list_parent = list->GetParent();
				list->Reparent(dlg_);

				wxVBox(dlg_, wxBOTTOM) = {
					wxSizerFlags(1).Expand() >>= list,
					new wxStaticLine(dlg_),
					dlg_->CreateStdDialogButtonSizer(wxOK),
				};

				dlg.SetPosition(ClientToScreen(wxPoint(GetPosition().x, GetPosition().y + GetSize().GetHeight())));
				dlg.SetMinSize(wxSize(GetSize().GetWidth(), -1));

				dlg.Fit();
				dlg.Layout();

				auto top_parent = wxGetTopLevelParent(this);

				if (top_parent) {
					top_parent->Bind(wxEVT_MOVE, &DiagCombo::OnMove, this);
				}

				Bind(wxEVT_SIZE, &DiagCombo::OnSize, this);

				Enable();
				dlg.ShowModal();

				Unbind(wxEVT_SIZE, &DiagCombo::OnSize, this);

				if (top_parent) {
					top_parent->Unbind(wxEVT_MOVE, &DiagCombo::OnMove, this);
				}

				list->Reparent(old_list_parent);
			};
		}
	};

	combo_ = new DiagCombo(this);
	combo_->SetPopupControl(new Popup());
#else
	combo_ = new wxComboCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxCC_STD_BUTTON | wxCB_READONLY);
	combo_->UseAltPopupWindow(true);

	combo_->SetPopupControl(new Popup(std::move(main_column_name), std::move(extra_column_names)));

#endif

	wxVBox(this, 0) = combo_;
}

void RearrangedItemsPickerCtrl::SetAvailableItems(std::vector<std::string> available)
{
	auto list = static_cast<Popup*>(combo_->GetPopupControl())->GetList();

	wxASSERT_MSG(list != nullptr, "The list has not been created.");

	list->SetAvailableItems(std::move(available));
	combo_->SetText(combo_->GetPopupControl()->GetStringValue());
}


void RearrangedItemsPickerCtrl::ClearActiveItems()
{
	auto list = static_cast<Popup*>(combo_->GetPopupControl())->GetList();

	wxASSERT_MSG(list != nullptr, "The list has not been created.");

	list->ClearActiveItems();
}

