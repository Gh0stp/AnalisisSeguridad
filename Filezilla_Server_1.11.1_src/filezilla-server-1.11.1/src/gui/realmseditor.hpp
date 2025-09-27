#ifndef REALMSEDITOR_HPP
#define REALMSEDITOR_HPP

#include <vector>

#include "../filezilla/authentication/realms.hpp"

#include <wx/panel.h>

#ifdef HAVE_CONFIG_H
#	include "config_modules.hpp"
#endif


class wxChoice;

class RealmsEditor: public wxPanel
{
public:
	enum policy_type {
		for_users,
		for_groups,
		for_system
	};

	RealmsEditor() = default;

	bool Create(wxWindow *parent, policy_type policy);

	/// \brief Sets the realms to be edited.
	/// *Doesn't* take ownership of the pointer.
	void SetRealms(std::vector<fz::authentication::realm> *realms);

	bool TransferDataFromWindow() override;
	bool TransferDataToWindow() override;

private:
	std::vector<fz::authentication::realm> *realms_{};
	policy_type policy_;

	wxChoice *ftp_choices_{};
	wxChoice *ftps_choices_{};

#ifdef ENABLE_FZ_WEBUI
	wxChoice *webui_choices_{};
#endif
};

#endif // REALMSEDITOR_HPP
