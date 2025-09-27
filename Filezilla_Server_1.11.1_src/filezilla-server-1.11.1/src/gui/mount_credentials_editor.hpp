#ifndef MOUNT_CREDENTIALS_EDITOR_HPP
#define MOUNT_CREDENTIALS_EDITOR_HPP

#include <wx/panel.h>

#include "../filezilla/tvfs/backend.hpp"

class wxSimplebook;
class RearrangedItemsPickerCtrl;

class MountCredentialsEditor: public wxPanel
{
public:
	MountCredentialsEditor(wxWindow *parent);

	void SetCredentials(fz::tvfs::backend::credentials_map  &creds);

private:
	class CredentialsEditor;
	class Manager;

	Manager *manager_;
};

#endif // MOUNT_CREDENTIALS_EDITOR_HPP
