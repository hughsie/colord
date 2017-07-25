cargs = ['-DG_LOG_DOMAIN="Cd"']

shared_module('cd-plugin-example',
  sources : [
    'cd-plugin-example.c',
  ],
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
    src_incdir,
  ],
  c_args : cargs,
  dependencies : [
    gio,
  ],
)
shared_module('cd-plugin-camera',
  sources : [
    'cd-plugin-camera.c',
  ],
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
    src_incdir,
  ],
  install : true,
  install_dir: join_paths(get_option('libdir'), 'colord-plugins'),
  c_args : cargs,
  dependencies : [
    gio,
    gudev,
  ],
)
shared_module('cd-plugin-scanner',
  sources : [
    'cd-plugin-scanner.c',
  ],
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
    src_incdir,
  ],
  install : true,
  install_dir: join_paths(get_option('libdir'), 'colord-plugins'),
  c_args : cargs,
  dependencies : [
    gio,
    gudev,
  ],
)
if get_option('enable-sane')
  shared_module('cd-plugin-sane',
    sources : [
      'cd-plugin-sane.c',
    ],
    include_directories : [
      root_incdir,
      root_incdir,
      src_incdir,
    ],
    install : true,
    install_dir: join_paths(get_option('libdir'), 'colord-plugins'),
    c_args : [
      cargs,
      '-DCOLORD_SANE_BINARY="' + join_paths(get_option('libexecdir'), 'colord-sane' + '"'),
    ],
    dependencies : [
      gio,
      gudev,
    ],
  )
endif
