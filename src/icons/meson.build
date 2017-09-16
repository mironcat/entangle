logo_icon_sizes = [
  '16x16',
  '32x32',
  '48x48',
  '64x64',
  '96x96',
  '128x128',
  '192x192',
  '256x256'
]

misc_icon_sizes = [
  '22x22',
  '48x48',
]

misc_icon_names = [
  'capture',
  'interface',
  'plugins',
  'color-management',
  'imageviewer',
]

foreach icon_size: logo_icon_sizes
  install_data(
    join_paths(icon_size, 'entangle.png'),
    install_dir: join_paths(entangle_datadir, 'icons', 'hicolor', icon_size, 'apps')
  )
endforeach

foreach icon_size: misc_icon_sizes
  foreach icon_name: misc_icon_names
    install_data(
      join_paths(icon_size, 'entangle-' + icon_name + '.png'),
      install_dir: join_paths(entangle_datadir, 'icons', 'hicolor', icon_size, 'apps')
    )
  endforeach
endforeach

install_data(
  join_paths('scalable', 'entangle.svg'),
  install_dir: join_paths(entangle_datadir, 'icons', 'hicolor', 'scalable', 'apps')
)
