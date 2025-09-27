#include <libfilezilla/glue/wx.hpp>

#include <wx/choicebk.h>
#include <wx/valgen.h>
#include <wx/statline.h>

#include "mount_credentials_editor.hpp"
#include "mappeditemmanager.hpp"

#include "helpers.hpp"
#include "textvalidatorex.hpp"

class MountCredentialsEditor::CredentialsEditor: public wxPanel
{
public:
	CredentialsEditor(wxWindow *parent)
		: wxPanel(parent)
	{
		auto *p = this;

		wxVBox(p) = {
			wxLabel(p, _S("&Share:")),
			path_ = new wxTextCtrl(p, nullID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY),

			wxLabel(p, _S("&Username:")),
			username_ = new wxTextCtrl(p, nullID),

			wxLabel(p, _S("&Password: (stored in plaintext)")),
			password_ = new wxTextCtrl(p, nullID, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD)
		};
	}

	void SetItem(fz::tvfs::backend::credentials_map::value_type *value)
	{
		value_ = value;

		if (value) {
			Enable();
			path_->SetValue(fz::to_wxString(value->first));
			username_->SetValidator(TextValidatorEx(wxFILTER_NONE, &value->second.username, field_must_not_be_empty(_S("Username"))));
			password_->SetValidator(TextValidatorEx(wxFILTER_NONE, &value->second.password));

		}
		else {
			Disable();
			path_->Clear();
			username_->SetValidator(wxValidator());
			password_->SetValidator(wxValidator());
		}

		CredentialsEditor::TransferDataToWindow();
	}

private:
	fz::tvfs::backend::credentials_map::value_type *value_{};
	wxTextCtrl *path_{};
	wxTextCtrl *username_{};
	wxTextCtrl *password_{};
};


/*********************************************************/

class MountCredentialsEditor::Manager: public MappedItemManager<fz::tvfs::backend::credentials_map, CredentialsEditor>
{
public:
	Manager(wxWindow *parent)
		: MappedItemManager(parent, fzT("\\\\Server\\Share"), _S("Available Shares"), _S("share"))
	{}

	bool is_name_allowed(const fz::native_string &name) override
	{
		if (!MappedItemManager::is_name_allowed(name)) {
			return {};
		}

		//TODO: at the moment we only support UNC paths
		fz::util::fs::absolute_windows_native_path canonical(name);

		return canonical.is_unc();
	}

	fz::native_string canonicalize_name(const fz::native_string &name) override
	{
		if (!MappedItemManager::is_name_allowed(name)) {
			return {};
		}

		//TODO: at the moment we only support UNC paths
		fz::util::fs::absolute_windows_native_path canonical(name);

		std::size_t root_pos;
		if (!canonical.is_unc(&root_pos)) {
			return {};
		}

		return canonical.str().substr(0, root_pos);
	}
};

MountCredentialsEditor::MountCredentialsEditor(wxWindow *parent)
	: wxPanel(parent, nullID)
{
	wxVBox(this, 0) = manager_ = new Manager(this);
}

void MountCredentialsEditor::SetCredentials(fz::tvfs::backend::credentials_map  &creds)
{
	manager_->SetMap(creds);
}
