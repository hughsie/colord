shared_module('cd-sensor-huey',
  sources : [
    'cd-sensor-huey.c',
    'huey-ctx.c',
    'huey-device.c',
    'huey-enum.c',
  ],
  include_directories : [
    src_incdir,
    colord_incdir,
    lib_incdir,
    root_incdir,
  ],
  install : true,
  install_dir: join_paths(get_option('libdir'), 'colord-sensors'),
  c_args : cargs,
  dependencies : [
    gio,
    gusb,
    gudev,
  ],
)
