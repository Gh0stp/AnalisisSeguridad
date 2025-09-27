#include <libfilezilla/glue/wx.hpp>

#include <wx/choicebk.h>

#include "realmseditor.hpp"
#include "helpers.hpp"

#ifdef HAVE_CONFIG_H
#	include "config_modules.hpp"
#endif

static wxChoice *create_choices(wxWindow *p, RealmsEditor::policy_type policy)
{
	return new wxChoice(p, nullID)  | [&](wxChoice * p) {
		switch (policy) {
			case RealmsEditor::for_groups:
				p->Append(_S("Determined by the other groups or system policies"));
				p->Append(_S("Enabled"));
				p->Append(_S("Disabled"));
				break;

			case RealmsEditor::for_users:
				p->Append(_S("Determined by groups or system policies"));
				p->Append(_S("Enabled – overrides groups policies"));
				p->Append(_S("Disabled – overrides groups policies"));
				break;

			case RealmsEditor::for_system:
				p->Append(_S("Enabled by default"));
				p->Append(_S("Disabled by default"));
				break;
		}

		p->SetSelection(0);
	};
}

bool RealmsEditor::Create(wxWindow *parent,  RealmsEditor::policy_type policy)
{
	if (!wxPanel::Create(parent, nullID, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER, wxS("RealmsEditor"))) {
		return false;
	}

	policy_ = policy;

	wxGBox(this, 2, {1}) = {
		wxLabel(this, _S("&FTP authentication (insecure):")),
		ftp_choices_ = create_choices(this, policy),

		wxLabel(this, _S("F&TPS (FTP over &TLS) authentication:")),
		ftps_choices_ = create_choices(this, policy),

#ifdef ENABLE_FZ_WEBUI
		wxLabel(this, _S("&WebUI authentication:")),
		webui_choices_ = create_choices(this, policy),
#endif
	};

	SetRealms(nullptr);

	return true;
}

void RealmsEditor::SetRealms(std::vector<fz::authentication::realm> *realms)
{
	realms_ = realms;
	TransferDataToWindow();
}

bool RealmsEditor::TransferDataFromWindow()
{
	if (!wxPanel::TransferDataFromWindow()) {
		return false;
	}

	if (realms_) {
		auto update = [this](const std::string &realm, wxChoice *c) {
			auto it = std::find_if(realms_->begin(), realms_->end(), [&](fz::authentication::realm &r) {
				return r.name == realm;
			});

			auto status = static_cast<enum fz::authentication::realm::status>(c->GetSelection() + (policy_ == for_system));

			if (it != realms_->end()) {
				if (!status) {
					realms_->erase(it);
				}
				else {
					it->status = status;
				}
			}
			else
			if (status) {
				realms_->push_back({realm, status});
			}
		};

		update("ftp", ftp_choices_);
		update("ftps", ftps_choices_);

#ifdef ENABLE_FZ_WEBUI
		update("webui", webui_choices_);
#endif
	}

	return true;
}

bool RealmsEditor::TransferDataToWindow()
{
	if (!wxPanel::TransferDataToWindow()) {
		return false;
	}

	auto update = [this](const std::string &realm, wxChoice *c) {
		if (!realms_) {
			c->Disable();
			c->SetSelection(0);
		}
		else {
			c->Enable();

			auto it = std::find_if(realms_->begin(), realms_->end(), [&](fz::authentication::realm &r) {
				return r.name == realm;
			});

			int sel{};

			if (it != realms_->end()) {
				sel = int(it->status);
			}

			wxASSERT_MSG(sel >= 0 && sel <= 2, "Realm status is invalid");

			if (policy_ == for_system) {
				sel = std::max(0, sel-1);
			}

			c->SetSelection(sel);
		}
	};

	update("ftp", ftp_choices_);
	update("ftps", ftps_choices_);

#ifdef ENABLE_FZ_WEBUI
	update("webui", webui_choices_);
#endif

	return true;
}
