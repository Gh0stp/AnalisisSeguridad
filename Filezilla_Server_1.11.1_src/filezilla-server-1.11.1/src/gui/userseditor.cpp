#include <libfilezilla/glue/wx.hpp>
#include <libfilezilla/util.hpp>

#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/splitter.h>
#include <wx/valtext.h>
#include <wx/checkbox.h>
#include <wx/msgdlg.h>
#include <wx/combo.h>
#include <wx/checklst.h>
#include <wx/rearrangectrl.h>
#include <wx/simplebook.h>

#include "rearrangeditemspickerctrl.hpp"
#include "userseditor.hpp"
#include "mounttableeditor.hpp"
#include "userslist.hpp"
#include "filterseditor.hpp"
#include "credentialseditor.hpp"
#include "limitseditor.hpp"
#include "realmseditor.hpp"
#include "locale.hpp"
#include "glue.hpp"

#include "helpers.hpp"

namespace {

enum page: size_t {
	enabled  = 0,
	disabled = 1
};

}

bool UsersEditor::Create(wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxString &name)
{
	if (!wxPanel::Create(parent, winid, pos, size, style, name))
		return false;

	wxHBox(this, 0) = {
		{ wxSizerFlags(0).Expand(), users_list_ = new UsersList(this) },
		{ wxSizerFlags(1).Expand(), book_ = wxCreate<wxNavigationEnabled<wxSimplebook>>(this) | [&](auto p) {
			wxPage<wxNotebook>(p, wxS("*Enabled*"), false, wxID_ANY) |[&](wxNotebook *p) {
				wxPage(p,  _S("General"), true) |[&](auto p) {
					wxVBox(p) = {
						enabled_check_ = new wxCheckBox(p, wxID_ANY, _S("User is enabled")),

						wxLabel(p, _S("Aut&hentication:")),
						credentials_editor_ = wxCreate<CredentialsEditor>(p),

						wxLabel(p, _S("Me&mber of groups:")),
						groups_chooser_ = wxCreate<RearrangedItemsPickerCtrl>(p),

						wxLabel(p, _S("Mount p&oints:")),
						wxSizerFlags(3) >>= mount_table_editor_ = wxCreate<MountTableEditor>(p),

						wxLabel(p, _S("Descr&iption:")),
						wxSizerFlags(1) >>= description_editor_ = new wxTextCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE)
					};
				};

				wxPage(p, _S("Filters")) |[&](auto p) {
					wxVBox(p) = filters_editor_ = wxCreate<FiltersEditor>(p);
				};

				wxPage(p, _S("Limits")) |[&](auto p) {
					wxVBox(p) = {
						limits_editor_ = wxCreate<LimitsEditor>(p),
					};
				};

				wxPage(p, _S("Protocol policies")) |[&](auto p) {
					wxVBox(p) = {
						realms_editor_ = wxCreate<RealmsEditor>(p, RealmsEditor::for_users),
					};
				};
			};

			wxPage(p, wxS("*Disabled*"), true) |[&](auto p) {
				wxVBox(p) = {
					wxEmptySpace,
					{ wxSizerFlags(0).Align(wxALIGN_CENTER), wxLabel(p, _S("Select or add a user in the list to the left")) },
						wxEmptySpace
					};
			};
		}}
	};

	users_list_->Bind(UsersList::Event::AboutToDeselectItem, [this](auto &ev){
		if (!Validate() || !TransferDataFromWindow())
			ev.Veto();
		else
			ev.Skip();
	});

	users_list_->Bind(UsersList::Event::SelectedItem, [this](auto &ev){
		ev.Skip();

		TransferDataToWindow();
	});

	credentials_editor_->Bind(CredentialsEditor::Event::ChangedMode, [this](CredentialsEditor::Event &ev) {
		mount_table_editor_->SetUserLogsInWithSystemCredentials(ev.GetMode() == CredentialsEditor::AuthMode::SystemPassword);
	});

	return true;
}

void UsersEditor::SetGroups(const fz::authentication::file_based_authenticator::groups &groups)
{
	fz::transformed_view group_names(groups, [](auto &g) -> const std::string& {
		return g.first;
	});

	std::vector<std::string> items;
	items.reserve(groups.size());

	for (const auto &n: group_names) {
		items.push_back(n);
	}

	groups_chooser_->SetAvailableItems(std::move(items));
}

void UsersEditor::SetServerPathFormat(fz::util::fs::path_format server_path_format)
{
	mount_table_editor_->SetNativePathFormat(server_path_format);
}

void UsersEditor::SetUsers(fz::authentication::file_based_authenticator::users &users, std::string server_name)
{
	users_ = &users;
	server_name_ = server_name;
	users_list_->SetMap(users);
}

bool UsersEditor::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow())
		return false;

	current_user_ = users_list_->GetSelectedItem();

	if (current_user_) {
		groups_chooser_->GetActiveItems(current_user_->second.groups);

		current_user_->second.description = fz::to_utf8(description_editor_->GetValue());
		current_user_->second.enabled = enabled_check_->GetValue();

		if (no_auth_)
			current_user_->second.methods = { {} };
		else
			current_user_->second.methods = { fz::authentication::method::password() };
	}

	return true;
}

bool UsersEditor::TransferDataToWindow()
{
	current_user_ = users_list_->GetSelectedItem();

	if (!current_user_) {
		credentials_editor_->SetCredentials(false, nullptr, nullptr, {});

		groups_chooser_->ClearActiveItems();
		mount_table_editor_->SetTable(nullptr);
		filters_editor_->SetIps(nullptr, nullptr);
		limits_editor_->SetSpeedLimits(nullptr);
		limits_editor_->SetTvfsLimits(nullptr);
		limits_editor_->SetSessionCountLimit(nullptr);
		realms_editor_->SetRealms(nullptr);
		description_editor_->ChangeValue(wxEmptyString);
	}
	else {
		bool is_system_user = current_user_->first == fz::authentication::file_based_authenticator::users::system_user_name;
		auto &u = current_user_->second;

		if (!u.credentials.is_valid_for(u.methods)) {
			u.enabled = false;
			u.methods = u.credentials.get_most_secure_methods();

			// Delay, don't do dialogs in TransferDataToWindow
			CallAfter([name = current_user_->first]() {
				wxMsg::Error(wxT("User \"%s\" configuration has become inconsisent."), name).Ext(wxT(
					"This user has been disabled for security.\n"
					"\n"
					"Before re-enabling this user, please check that the authentication methods and credentials are set as intended."
				));
			});
		}

		enabled_check_->SetValue(u.enabled);
		no_auth_ = !u.methods.is_auth_necessary();

		credentials_editor_->SetCredentials(is_system_user, &current_user_->second.credentials, &no_auth_, std::string(current_user_->first).append("@").append(server_name_));

		groups_chooser_->SetActiveItems(current_user_->second.groups);
		mount_table_editor_->SetTable(&current_user_->second.mount_table);
		mount_table_editor_->SetUserLogsInWithSystemCredentials(current_user_->second.credentials.password.get_impersonation());
		filters_editor_->SetIps(&current_user_->second.disallowed_ips, &current_user_->second.allowed_ips);
		limits_editor_->SetSpeedLimits(&current_user_->second.rate_limits);
		limits_editor_->SetTvfsLimits(&current_user_->second.session_open_limits);
		limits_editor_->SetSessionCountLimit(&current_user_->second.session_count_limit);
		realms_editor_->SetRealms(&current_user_->second.realms);
		description_editor_->ChangeValue(fz::to_wxString(current_user_->second.description));
	}

	wxPanel::TransferDataToWindow();

	book_->ChangeSelection(current_user_ ? page::enabled : page::disabled);

	return true;
}
