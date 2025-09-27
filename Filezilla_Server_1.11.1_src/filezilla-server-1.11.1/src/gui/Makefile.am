bin_PROGRAMS = filezilla-server-gui

filezilla_server_gui_SOURCES_H = \
	../server/xml_upgrader.hpp \
	../tools/configconverter/converter.hpp \
	../tools/configconverter/server_config.hpp \
	addressinfolisteditor.hpp \
	binaryaddresslisteditor.hpp \
	credentialseditor.hpp \
	dialogex.hpp \
	downloader.hpp \
	ftptester.hpp \
	glue.hpp \
	gui_config_paths.hpp \
	limitseditor.hpp \
	locale.hpp \
	eventex.hpp \
	fluidcolumnlayoutmanager.hpp \
	certinfoeditor.hpp \
	filterseditor.hpp \
	generatecertificatedialog.hpp \
	mount_credentials_editor.hpp \
	mappeditemmanager.hpp \
	nameditemlist.hpp \
	networkconfigwizard.hpp \
	passwordeditor.hpp \
	realmseditor.hpp \
	rearrangedlist.hpp \
	serverlogger.hpp \
	settings.hpp \
	helpers.hpp \
	integraleditor.hpp \
	listctrlex.hpp \
	rearrangeditemspickerctrl.hpp \
	connectiondialog.hpp \
	groupseditor.hpp \
	groupslist.hpp \
	mounttableeditor.hpp \
	serveradministrator.hpp \
	sessionlist.hpp \
	settingsdialog.hpp \
	tls_param_editor.hpp \
	trayicon.hpp \
	userseditor.hpp \
	userslist.hpp \
	textvalidatorex.hpp \
	wrappedtext.hpp

filezilla_server_gui_SOURCES = \
    $(filezilla_server_gui_SOURCES_H) \
	../server/xml_upgrader.cpp \
	../tools/configconverter/converter.cpp \
	addressinfolisteditor.cpp \
	binaryaddresslisteditor.cpp \
	credentialseditor.cpp \
	downloader.cpp \
	ftptester.cpp \
	limitseditor.cpp \
	main.cpp \
	fluidcolumnlayoutmanager.cpp \
	certinfoeditor.cpp \
	filterseditor.cpp \
	generatecertificatedialog.cpp \
	mount_credentials_editor.cpp \
	nameditemlist.cpp \
	networkconfigwizard.cpp \
	passwordeditor.cpp \
	realmseditor.cpp \
	rearrangedlist.cpp \
	serveradministrator/acme.cpp \
	serveradministrator/certificates.cpp \
	serveradministrator/configure.cpp \
	serveradministrator/groups_and_users.cpp \
	serveradministrator/imexport.cpp \
	serveradministrator/updates.cpp \
	serverlogger.cpp \
	settings.cpp \
	helpers.cpp \
	integraleditor.cpp \
	listctrlex.cpp \
	rearrangeditemspickerctrl.cpp \
	connectiondialog.cpp \
	groupseditor.cpp \
	groupslist.cpp \
	mounttableeditor.cpp \
	serveradministrator.cpp \
	sessionlist.cpp \
	settingsdialog.cpp \
	tls_param_editor.cpp \
	trayicon.cpp \
	userseditor.cpp \
	userslist.cpp \
	textvalidatorex.cpp \
	wrappedtext.cpp

if FZ_WINDOWS
    EXTRA_CXXFLAGS = -DwxCRT_StrdupA=strdup
    EXTRA_LDADD = $(top_builddir)/res/filezilla-server-gui.o
    EXTRA_LIBS =
endif

filezilla_server_gui_CXXFLAGS = $(LIBFILEZILLA_CFLAGS) $(WX_CXXFLAGS) $(EXTRA_CXXFLAGS) -fno-exceptions
filezilla_server_gui_LDADD    = $(EXTRA_LDADD) ../filezilla/libfilezilla-common.a $(LIBFILEZILLA_LIBS) $(WX_LIBS) $(EXTRA_LIBS) $(PUGIXML_LIBS)

