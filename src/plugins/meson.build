cargs = ['-DG_LOG_DOMAIN="Cd"']

shared_module('colord_sensor_example',
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
shared_module('colord_sensor_camera',
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
  install_dir: join_paths(libdir, 'colord-plugins'),
  c_args : cargs,
  dependencies : [
    gio,
    gudev,
  ],
)
shared_module('colord_sensor_scanner',
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
  install_dir: join_paths(libdir, 'colord-plugins'),
  c_args : cargs,
  dependencies : [
    gio,
    gudev,
  ],
)
if get_option('sane')
  shared_module('colord_sensor_sane',
    sources : [
      'cd-plugin-sane.c',
    ],
    include_directories : [
      colord_incdir,
      lib_incdir,
      root_incdir,
      src_incdir,
    ],
    install : true,
    install_dir: join_paths(libdir, 'colord-plugins'),
    c_args : [
      cargs,
      '-DCOLORD_SANE_BINARY="' + join_paths(libexecdir, 'colord-sane') + '"',
    ],
    dependencies : [
      gio,
      gudev,
    ],
  )
endif
