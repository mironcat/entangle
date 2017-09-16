doc_path = join_paths(entangle_datadir, 'gtk-doc', 'html', meson.project_name())


gnome.gtkdoc(
  meson.project_name(),
  main_xml: meson.project_name() + '-docs.xml',
  src_dir: [
    backend_inc,
    frontend_inc,
  ],
  dependencies: [
    entangle_backend_dep,
    entangle_frontend_dep,
  ],
  gobject_typesfile: 'entangle.types',
  scan_args: [
    '--rebuild-sections',
    '--rebuild-types',
  ],
  mkdb_args: [
    '--output-format=xml',
  ],
  install: true,
  install_dir: doc_path
)
