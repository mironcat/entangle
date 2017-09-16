image_display_enum_headers = files(
  'entangle-image-display.h',
)

entangle_frontend_sources = [
  'entangle-application.c',
  'entangle-camera-support.c',
  'entangle-camera-manager.c',
  'entangle-camera-picker.c',
  'entangle-camera-preferences.c',
  'entangle-control-panel.c',
  'entangle-dpms.c',
  'entangle-help-about.c',
  'entangle-image-display.c',
  'entangle-image-histogram.c',
  'entangle-media-popup.c',
  'entangle-media-statusbar.c',
  'entangle-preferences.c',
  'entangle-preferences-display.c',
  'entangle-script.c',
  'entangle-script-simple.c',
  'entangle-script-config.c',
  'entangle-session-browser.c'
]

entangle_frontend_headers = [
  'entangle-application.h',
  'entangle-camera-support.h',
  'entangle-camera-manager.h',
  'entangle-camera-picker.h',
  'entangle-camera-preferences.h',
  'entangle-control-panel.h',
  'entangle-dpms.h',
  'entangle-help-about.h',
  'entangle-image-display.h',
  'entangle-image-histogram.h',
  'entangle-media-popup.h',
  'entangle-media-statusbar.h',
  'entangle-preferences.h',
  'entangle-preferences-display.h',
  'entangle-script.h',
  'entangle-script-simple.h',
  'entangle-script-config.h',
  'entangle-session-browser.h'
]

entangle_frontend_files = []
foreach f: entangle_frontend_sources + entangle_frontend_headers
  fn = join_paths('frontend', f)
  entangle_frontend_files += [fn]
endforeach


entangle_frontend_enums = []

entangle_frontend_enums += gnome.mkenums(
  'entangle-image-display-enums.h',
  sources: image_display_enum_headers,
  comments: '/* @comment@ */',
  fhead: '#ifndef __ENTANGLE_FRONTEND_ENUMS_H__\n#define __ENTANGLE_FRONTEND_ENUMS_H__\n\n#include <glib-object.h>\nG_BEGIN_DECLS\n\n',
  ftail: 'G_END_DECLS\n\n#endif /* __ENTANGLE_FRONTEND_ENUMS_H__ */\n',
  fprod: '\n/* --- @filename@ --- */',
  eprod: '#define ENTANGLE_TYPE_@ENUMSHORT@ @enum_name@_get_type()\nGType @enum_name@_get_type (void);\n',
  install_header: false,
)

entangle_frontend_enums += gnome.mkenums(
  'entangle-image-display-enums.c',
  sources: image_display_enum_headers,
  comments: '/* @comment@ */',
  fhead: '#include "entangle-image-display-enums.h"\n\n',
  fprod: '\n/* enumerations from "@filename@" */\n#include "@filename@"\n',
  vhead: 'static const G@Type@Value _@enum_name@_values[] = {',
  vprod: '  { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '  { 0, NULL, NULL }\n};\n\nGType\n@enum_name@_get_type (void)\n{\n  static GType type = 0;\n\n  if (!type)\n    type = g_@type@_register_static ("@EnumName@", _@enum_name@_values);\n\n  return type;\n}\n\n',
  install_header: false,
)

entangle_frontend_sources += entangle_frontend_enums

entangle_frontend_deps = [
  gthread_dep,
  gmodule_dep,
  gtk_dep,
  gst_dep,
  libpeas_dep,
  libpeas_gtk_dep,
  gexiv2_dep,
  xext_dep,
  entangle_backend_dep,
]

entangle_frontend_cflags = [
    '-DDATADIR="' + entangle_datadir + '"',
    '-DPKGDATADIR="' + entangle_pkgdatadir + '"',
    '-DLIBDIR="' + entangle_libdir + '"',
    '-DVERSION="' + meson.project_version() + '"',
]

entangle_frontend = shared_library(
  'entangle_frontend', entangle_frontend_enums,
  sources: entangle_frontend_sources,
  dependencies: entangle_frontend_deps,
  include_directories: backend_inc,
  c_args: entangle_frontend_cflags,
  version: '0.0.0',
  install: true,
  install_dir: entangle_libdir,
)

entangle_frontend_dep = declare_dependency(
  link_with: entangle_frontend,
  dependencies: entangle_frontend_deps,
)
