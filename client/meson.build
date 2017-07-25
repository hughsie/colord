cargs = ['-DG_LOG_DOMAIN="Cd"']

executable(
  'colormgr',
  sources : [
    'cd-util.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../lib'),
    include_directories('../lib/colord'),
  ],
  dependencies : [
    gio,
    lcms,
    libm,
  ],
  link_with : colord,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : get_option('bindir')
)

executable(
  'cd-fix-profile',
  sources : [
    'cd-fix-profile.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../lib'),
    include_directories('../lib/colord'),
  ],
  dependencies : [
    gio,
    lcms,
    libm,
  ],
  link_with : colord,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : get_option('bindir')
)

executable(
  'cd-iccdump',
  sources : [
    'cd-iccdump.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../lib'),
    include_directories('../lib/colord'),
  ],
  dependencies : [
    gio,
    lcms,
    libm,
  ],
  link_with : colord,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : get_option('bindir')
)

cd_idt8 = executable(
  'cd-it8',
  sources : [
    'cd-it8.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../lib'),
    include_directories('../lib/colord'),
  ],
  dependencies : [
    gio,
    lcms,
    libm,
  ],
  link_with : colord,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : get_option('bindir')
)

cd_create_profile = executable(
  'cd-create-profile',
  sources : [
    'cd-create-profile.c',
  ],
  include_directories : [
    include_directories('..'),
    include_directories('../lib'),
    include_directories('../lib/colord'),
  ],
  dependencies : [
    gio,
    lcms,
    libm,
  ],
  link_with : colordprivate,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : get_option('bindir')
)
