install_headers(
  'cd-compat-edid.h',
  subdir : 'colord-1/colord',
)

shared_module('colordcompat',
  sources : [
    'cd-compat-edid.c',
  ],
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
  ],
  install : true,
  install_dir: libdir,
  c_args : cargs,
  link_with : colord,
  dependencies : [
    gio,
    gudev,
  ],
)
