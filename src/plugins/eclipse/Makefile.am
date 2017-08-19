
pluginlibdir = $(libdir)/entangle/plugins/eclipse
pluginlib_DATA = eclipse.plugin eclipse.py

settingsdir = $(datadir)/entangle/plugins/eclipse/schemas
settings_DATA = org.entangle-photo.plugins.eclipse.gschema.xml

EXTRA_DIST = $(pluginlib_DATA) $(settings_DATA)

install-data-hook:
	$(MKDIR_P) $(DESTDIR)$(settingsdir)
	$(GLIB_COMPILE_SCHEMAS) $(DESTDIR)$(settingsdir)/

uninstall-local:
	rm -f $(DESTDIR)/$(settingsdir)/$(settings_DATA) || :
	rm -f $(DESTDIR)/$(settingsdir)/gschemas.compiled || :
	rmdir $(DESTDIR)/$(settingsdir) || :
