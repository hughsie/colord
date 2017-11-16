shared_module('colord_sensor_colorhug',
  sources : [
    'cd-sensor-colorhug.c',
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
  link_with : colorhug,
  dependencies : [
    gio,
    gusb,
    gudev,
  ],
)
