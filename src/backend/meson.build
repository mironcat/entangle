
camera_enum_headers = files(
  'entangle-camera.h',
)
colour_profile_enum_headers = files(
  'entangle-colour-profile.h',
)

entangle_backend_sources = [
  'entangle-media.c',
  'entangle-image.c',
  'entangle-video.c',
  'entangle-session.c',
  'entangle-camera.c',
  'entangle-camera-automata.c',
  'entangle-camera-file.c',
  'entangle-camera-list.c',
  'entangle-colour-profile.c',
  'entangle-control.c',
  'entangle-control-button.c',
  'entangle-control-choice.c',
  'entangle-control-date.c',
  'entangle-control-group.c',
  'entangle-control-range.c',
  'entangle-control-text.c',
  'entangle-control-toggle.c',
  'entangle-debug.c',
  'entangle-device-manager.c',
  'entangle-pixbuf.c',
  'entangle-image-loader.c',
  'entangle-pixbuf-loader.c',
  'entangle-progress.c',
  'entangle-thumbnail-loader.c',
]

entangle_backend_headers = [
  'entangle-media.h',
  'entangle-image.h',
  'entangle-video.h',
  'entangle-session.h',
  'entangle-camera.h',
  'entangle-camera-automata.h',
  'entangle-camera-file.h',
  'entangle-camera-list.h',
  'entangle-colour-profile.h',
  'entangle-control.h',
  'entangle-control-button.h',
  'entangle-control-choice.h',
  'entangle-control-date.h',
  'entangle-control-group.h',
  'entangle-control-range.h',
  'entangle-control-text.h',
  'entangle-control-toggle.h',
  'entangle-debug.h',
  'entangle-device-manager.h',
  'entangle-pixbuf.h',
  'entangle-image-loader.h',
  'entangle-pixbuf-loader.h',
  'entangle-progress.h',
  'entangle-thumbnail-loader.h',
]

entangle_backend_files = []
foreach f: entangle_backend_sources + entangle_backend_headers
  fn = join_paths('backend', f)
  entangle_backend_files += [fn]
endforeach

entangle_backend_enums = []

entangle_backend_enums += gnome.mkenums(
  'entangle-camera-enums.h',
  sources: camera_enum_headers,
  comments: '/* @comment@ */',
  fhead: '#ifndef __ENTANGLE_BACKEND_ENUMS_H__\n#define __ENTANGLE_BACKEND_ENUMS_H__\n\n#include <glib-object.h>\nG_BEGIN_DECLS\n\n',
  ftail: 'G_END_DECLS\n\n#endif /* __ENTANGLE_BACKEND_ENUMS_H__ */\n',
  fprod: '\n/* --- @filename@ --- */',
  eprod: '#define ENTANGLE_TYPE_@ENUMSHORT@ @enum_name@_get_type()\nGType @enum_name@_get_type (void);\n',
  install_header: false,
)

entangle_backend_enums += gnome.mkenums(
  'entangle-camera-enums.c',
  sources: camera_enum_headers,
  comments: '/* @comment@ */',
  fhead: '#include "entangle-camera-enums.h"\n\n',
  fprod: '\n/* enumerations from "@filename@" */\n#include "@filename@"\n',
  vhead: 'static const G@Type@Value _@enum_name@_values[] = {',
  vprod: '  { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '  { 0, NULL, NULL }\n};\n\nGType\n@enum_name@_get_type (void)\n{\n  static GType type = 0;\n\n  if (!type)\n    type = g_@type@_register_static ("@EnumName@", _@enum_name@_values);\n\n  return type;\n}\n\n',
  install_header: false,
)

entangle_backend_enums += gnome.mkenums(
  'entangle-colour-profile-enums.h',
  sources: colour_profile_enum_headers,
  comments: '/* @comment@ */',
  fhead: '#ifndef __ENTANGLE_BACKEND_ENUMS_H__\n#define __ENTANGLE_BACKEND_ENUMS_H__\n\n#include <glib-object.h>\nG_BEGIN_DECLS\n\n',
  ftail: 'G_END_DECLS\n\n#endif /* __ENTANGLE_BACKEND_ENUMS_H__ */\n',
  fprod: '\n/* --- @filename@ --- */',
  eprod: '#define ENTANGLE_TYPE_@ENUMSHORT@ @enum_name@_get_type()\nGType @enum_name@_get_type (void);\n',
  install_header: false,
)

entangle_backend_enums += gnome.mkenums(
  'entangle-colour-profile-enums.c',
  sources: colour_profile_enum_headers,
  comments: '/* @comment@ */',
  fhead: '#include "entangle-colour-profile-enums.h"\n\n',
  fprod: '\n/* enumerations from "@filename@" */\n#include "@filename@"\n',
  vhead: 'static const G@Type@Value _@enum_name@_values[] = {',
  vprod: '  { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '  { 0, NULL, NULL }\n};\n\nGType\n@enum_name@_get_type (void)\n{\n  static GType type = 0;\n\n  if (!type)\n    type = g_@type@_register_static ("@EnumName@", _@enum_name@_values);\n\n  return type;\n}\n\n',
  install_header: false,
)

entangle_backend_sources += entangle_backend_enums

entangle_backend_deps = [
  glib_dep,
  gio_dep,
  gthread_dep,
  gdk_pixbuf_dep,
  gst_dep,
  gst_video_dep,
  gphoto2_dep,
  lcms2_dep,
  gudev_dep,
  libpeas_dep,
  gexiv2_dep,
  libraw_dep,
]

entangle_backend = shared_library(
  'entangle_backend', entangle_backend_enums,
  sources: entangle_backend_sources,
  dependencies: entangle_backend_deps,
  version: '0.0.0',
  install: true,
  install_dir: entangle_libdir,
)

entangle_backend_dep = declare_dependency(
  link_with: entangle_backend,
  dependencies: entangle_backend_deps,
)
