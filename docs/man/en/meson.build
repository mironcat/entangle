
man = custom_target(
    'man',
    output : 'entangle.1',
    input : 'entangle.pod',
    command : [
      pod2man,
      '--section=1',
      '--center=User Commands',
      '--name=ENTANGLE',
      '--release=Entangle @0@'.format(meson.project_version()),
      '@INPUT@', '@OUTPUT@'
    ],
    install : true,
    install_dir : join_paths(get_option('mandir'), 'man1')
)
