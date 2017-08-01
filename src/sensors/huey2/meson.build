cargs = ['-DG_LOG_DOMAIN="CdSensorHuey2"']

executable(
  'huey2-cmd',
  sources : [
    'huey-device.c',
    'huey-tool.c',
  ],
  include_directories : [
      colord_incdir,
      lib_incdir,
      root_incdir,
  ],
  dependencies : [
    gio,
    gusb,
    libm,
  ],
  link_with : colord,
  c_args : [
    cargs,
  ],
)
