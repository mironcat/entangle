
plugin_sources = [
  'eclipse.plugin',
  'eclipse.py',
]

plugin_schemas = [
  'org.entangle-photo.plugins.eclipse.gschema.xml',
]

install_data(
  sources: plugin_sources,
  install_dir: join_paths(entangle_pluginlibdir, 'eclipse')
)

install_data(
  sources: plugin_schemas,
  install_dir: join_paths(entangle_plugindatadir, 'eclipse', 'schemas')
)
