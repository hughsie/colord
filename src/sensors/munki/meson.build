if get_option('reverse')
  shared_module('colord_sensor_munki',
    sources : [
      'cd-sensor-munki.c',
      'munki-enum.c',
    ],
    include_directories : [
      src_incdir,
      colord_incdir,
      lib_incdir,
      root_incdir,
    ],
    c_args : cargs,
    dependencies : [
      gio,
      gusb,
      gudev,
    ],
  )
endif
