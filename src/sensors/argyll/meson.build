shared_module('colord_sensor_argyll',
  sources : [
    'cd-sensor-argyll.c',
    'cd-spawn.c',
  ],
  include_directories : [
    src_incdir,
    colord_incdir,
    lib_incdir,
    root_incdir,
  ],
  install : true,
  install_dir: join_paths(libdir, 'colord-sensors'),
  c_args : cargs,
  dependencies : [
    gio,
    gusb,
    gudev,
  ],
)
