#!/usr/bin/env python3

import os
import subprocess

install_prefix = os.environ['MESON_INSTALL_PREFIX']
install_destdir_prefix = os.environ['MESON_INSTALL_DESTDIR_PREFIX']

icondir = os.path.join(install_destdir_prefix, 'share', 'icons', 'hicolor')
schemadir = os.path.join(install_destdir_prefix, 'share', 'glib-2.0', 'schemas')
plugindir = os.path.join(install_destdir_prefix, 'share', 'entangle', 'plugins')

if install_destdir_prefix == install_prefix:
  print('Update icon cache...')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

  print('Compiling gsettings schemas...')
  subprocess.call(['glib-compile-schemas', schemadir])

for plugin in os.listdir(plugindir):
  print('Compiling %s gsettings schemas...' % plugin)
  pluginschemadir = os.path.join(plugindir, plugin, 'schemas')
  subprocess.call(['glib-compile-schemas', pluginschemadir])
