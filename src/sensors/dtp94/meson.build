shared_module('cd-sensor-dtp94',
  sources : [
    'cd-sensor-dtp94.c',
    'dtp94-device.c',
    'dtp94-enum.c',
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
