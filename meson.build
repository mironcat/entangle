project(
  'entangle','c',
  version: '1.0',
  license: 'GPLv3+',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu99',
    'warning_level=1',
  ],
  meson_version: '>= 0.41.0'
)

pod2man = find_program('pod2man')

cc = meson.get_compiler('c')

glib_min_version = '>= 2.36.0'
gdk_pixbuf_min_version = '>= 2.12.0'
gtk_min_version = '>= 3.12.0'
gphoto2_min_version = '>= 2.5.0'
gudev_min_version = '>= 145'
gobject_introspection_min_version = '>= 0.9.3'
lcms2_min_version = '>= 2.0'
libpeas_min_version = '>= 1.2.0'
gexiv2_min_version = '>= 0.10'
xext_min_version = '>= 1.3.0'
libraw_min_version = '>= 0.9.0'
gst_min_version = '>= 1.0.0'

glib_dep = dependency('glib-2.0', version: glib_min_version)
gio_dep = dependency('gio-2.0', version: glib_min_version)
gthread_dep = dependency('gthread-2.0', version: glib_min_version)
gmodule_dep = dependency('gmodule-2.0', version: glib_min_version)
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: gdk_pixbuf_min_version)
gtk_dep = dependency('gtk+-3.0', version: gtk_min_version)
gphoto2_dep = dependency('libgphoto2', version: gphoto2_min_version)
gudev_dep = dependency('gudev-1.0', version: gudev_min_version)
gobject_introspection_dep = dependency('gobject-introspection-1.0', version: gobject_introspection_min_version)
lcms2_dep = dependency('lcms2', version: lcms2_min_version)
libpeas_dep = dependency('libpeas-1.0', version: libpeas_min_version)
libpeas_gtk_dep = dependency('libpeas-gtk-1.0', version: libpeas_min_version)
gexiv2_dep = dependency('gexiv2', version: gexiv2_min_version)
xext_dep = dependency('xext', version: xext_min_version)
#libraw_dep = dependency('libraw_r', version: libraw_min_version)
libraw_dep = cc.find_library('libraw_r')
gst_dep = dependency('gstreamer-1.0', version: gst_min_version)
gst_video_dep = dependency('gstreamer-video-1.0', version: gst_min_version)

gnome = import('gnome')
i18n = import('i18n')

check_cflags = [
  '-W',
  '-Waddress',
  '-Waggressive-loop-optimizations',
  '-Wall',
  '-Warray-bounds',
  '-Wattributes',
  '-Wbuiltin-macro-redefined',
  '-Wcast-align',
  '-Wchar-subscripts',
  '-Wclobbered',
  '-Wcomment',
  '-Wcomments',
  '-Wcoverage-mismatch',
  '-Wcpp',
  '-Wdate-time',
  '-Wdeprecated-declarations',
  '-Wdisabled-optimization',
  '-Wdiv-by-zero',
  '-Wdouble-promotion',
  '-Wempty-body',
  '-Wendif-labels',
  '-Wenum-compare',
  '-Wextra',
  '-Wformat-contains-nul',
  '-Wformat-extra-args',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wformat-zero-length',
  '-Wfree-nonheap-object',
  '-Wignored-qualifiers',
  '-Wimplicit',
  '-Wimplicit-function-declaration',
  '-Wimplicit-int',
  '-Winit-self',
  '-Winline',
  '-Wint-to-pointer-cast',
  '-Winvalid-memory-model',
  '-Winvalid-pch',
  '-Wjump-misses-init',
  '-Wlogical-op',
  '-Wmain',
  '-Wmaybe-uninitialized',
  '-Wmissing-braces',
  '-Wmissing-declarations',
  '-Wmissing-field-initializers',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wmultichar',
  '-Wnarrowing',
  '-Wnested-externs',
  '-Wnonnull',
  '-Wold-style-declaration',
  '-Wold-style-definition',
  '-Wopenmp-simd',
  '-Woverflow',
  '-Woverlength-strings',
  '-Woverride-init',
  '-Wpacked',
  '-Wpacked-bitfield-compat',
  '-Wparentheses',
  '-Wpointer-arith',
  '-Wpointer-sign',
  '-Wpointer-to-int-cast',
  '-Wpragmas',
  '-Wreturn-local-addr',
  '-Wreturn-type',
  '-Wsequence-point',
  '-Wshadow',
  '-Wsizeof-pointer-memaccess',
  '-Wstack-protector',
  '-Wstrict-aliasing',
  '-Wstrict-overflow',
  '-Wstrict-prototypes',
  '-Wsuggest-attribute=const',
  '-Wsuggest-attribute=format',
  '-Wsuggest-attribute=noreturn',
  '-Wsuggest-attribute=pure',
  '-Wswitch',
  '-Wswitch-default',
  '-Wsync-nand',
  '-Wtrampolines',
  '-Wtrigraphs',
  '-Wtype-limits',
  '-Wuninitialized',
  '-Wunknown-pragmas',
  '-Wunsafe-loop-optimizations',
  '-Wunused',
  '-Wunused-but-set-parameter',
  '-Wunused-but-set-variable',
  '-Wunused-function',
  '-Wunused-label',
  '-Wunused-local-typedefs',
  '-Wunused-macros',
  '-Wunused-parameter',
  '-Wunused-result',
  '-Wunused-value',
  '-Wunused-variable',
  '-Wvarargs',
  '-Wvariadic-macros',
  '-Wvector-operation-performance',
  '-Wvla',
  '-Wvolatile-register-var',
  '-Wwrite-strings',
  '-Wnormalized=nfc',
  '-Wno-sign-compare',
  '-Wno-sign-conversion',
  '-Wno-conversion',
  '-Wno-unsafe-loop-optimizations',
  '-Wjump-misses-init',
  '-Wframe-larger-than=40096',
  '-O2',
  '-Wp,-D_FORTIFY_SOURCE=2',
  '-fstack-protector-all',
  '--param=ssp-buffer-size=4',
  '-fexceptions',
  '-fasynchronous-unwind-tables',
  '-fdiagnostics-show-option',
  '-funit-at-a-time',
  '-fipa-pure-const',
  '-Wno-suggest-attribute=pure',
  '-Wno-suggest-attribute=const',
]

cflags = []
foreach cflag: check_cflags
  if cc.has_argument(cflag)
    cflags += [cflag]
  endif
endforeach

werror_option = get_option('enable-werror')
if werror_option
  if cc.has_argument('-Werror')
    cflags += ['-Werror']
  endif
endif


add_project_arguments(cflags, language: 'c')

entangle_prefix = get_option('prefix')
entangle_bindir = join_paths(entangle_prefix, get_option('bindir'))
entangle_libdir = join_paths(entangle_prefix, get_option('libdir'))
entangle_localedir = join_paths(entangle_prefix, get_option('localedir'))
entangle_datadir = join_paths(entangle_prefix, get_option('datadir'))

entangle_pkgdatadir = join_paths(entangle_datadir, meson.project_name())
entangle_schemadir = join_paths(entangle_datadir, 'glib-2.0', 'schemas')
entangle_pluginlibdir = join_paths(entangle_libdir, 'entangle', 'plugins')
entangle_plugindatadir = join_paths(entangle_datadir, 'entangle', 'plugins')

po_dir = join_paths(meson.source_root(), 'po')

subdir('src')
subdir('po')
subdir('help')
subdir('docs')

meson.add_install_script('meson_post_install.py')
