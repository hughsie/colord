cargs = ['-DG_LOG_DOMAIN="Cd"']

executable(
  'colord-sane',
  sources : [
    'cd-main.c',
  ],
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
  ],
  dependencies : [
    dbus,
    gio,
    gudev,
    sane,
  ],
  link_with : colord,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : libexecdir
)
