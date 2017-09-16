
plugin_sources = [
  'photobox.plugin',
  'photobox.py',
]

plugin_schemas = [
  'org.entangle-photo.plugins.photobox.gschema.xml',
]

install_data(
  sources: plugin_sources,
  install_dir: join_paths(entangle_pluginlibdir, 'photobox')
)

install_data(
  sources: plugin_schemas,
  install_dir: join_paths(entangle_plugindatadir, 'photobox', 'schemas')
)
