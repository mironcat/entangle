
backend_inc = include_directories('backend')
frontend_inc = include_directories('frontend')

subdir('backend')
subdir('frontend')
subdir('icons')
subdir('plugins')

entangle_sources = [
  'entangle-main.c'
]

entangle_sources += gnome.compile_resources(
  'entangle-resources',
  'entangle.gresource.xml',
  source_dir: '.',
)

entangle_deps = [
  entangle_frontend_dep,
]

entangle_cflags = [
    '-DDATADIR="' + entangle_datadir + '"',
    '-DLOCALE_DIR="' + entangle_localedir + '"',
    '-DGETTEXT_PACKAGE="entangle"',
]

entangle = executable(
  'entangle',
  entangle_sources,
  dependencies: entangle_deps,
  include_directories: [backend_inc, frontend_inc],
  c_args: entangle_cflags,
  install: true,
  install_dir: entangle_bindir
)

desktop = 'entangle.desktop'

i18n.merge_file (
  desktop,
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(entangle_datadir, 'applications')
)

appdata = 'entangle.appdata.xml'

i18n.merge_file(
  appdata,
  input: appdata + '.in',
  output: appdata,
  install: true,
  install_dir: join_paths(entangle_datadir, 'appdata'),
  po_dir: po_dir,
)


schema = 'org.entangle-photo.manager.gschema.xml'

install_data(
  schema,
  install_dir: entangle_schemadir,
)


srgb = 'sRGB.icc'

install_data(
  srgb,
  install_dir: entangle_pkgdatadir,
)

gir_sources = entangle_frontend_files + entangle_backend_files

gir_incs = [
  'GObject-2.0',
  'Gtk-3.0',
  'Peas-1.0',
  'Gst-1.0',
  'GstBase-1.0',
  'GExiv2-0.10',
]

gir_dir = join_paths(entangle_datadir, 'gir-1.0')
typelib_dir = join_paths(entangle_libdir, 'girepository-1.0')

entangle_gir = gnome.generate_gir(
    entangle,
    sources: gir_sources,
    nsversion: '0.1',
    namespace: 'Entangle',
    includes: gir_incs,
    include_directories: [
      backend_inc,
      frontend_inc,
    ],
    install: true,
    install_dir_gir: gir_dir,
    install_dir_typelib: typelib_dir,
  )
