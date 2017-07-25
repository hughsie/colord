cargs = [
  '-DG_LOG_DOMAIN="libcolorhug"',
]

colorhug_version_h = configure_file(
  input : 'ch-version.h.in',
  output : 'ch-version.h',
  configuration : conf,
  install : true,
  install_dir: join_paths(get_option('includedir'), 'colord-1/colorhug'),
)

install_headers(
    'ch-common.h',
    'ch-device.h',
    'ch-device-queue.h',
    'ch-hash.h',
    'ch-inhx32.h',
    'ch-math.h',
    'colorhug.h',
  subdir : 'colord-1/colorhug',
)

mapfile = 'colorhug.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
colorhug = shared_library(
  'colorhug',
  sources : [
    'ch-common.c',
    'ch-device.c',
    'ch-device-queue.c',
    'ch-hash.c',
    'ch-inhx32.c',
    'ch-math.c',
  ],
  soversion : lt_current,
  version : lt_version,
  dependencies : [
    giounix,
    gusb,
    lcms,
    libm,
    libudev,
  ],
  c_args : [
      cargs,
      '-DLOCALSTATEDIR="' + localstatedir + '"',
    ],
  link_with : colordprivate,
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
  ],
  link_args : vflag,
  link_depends : mapfile,
  install : true
)

pkgg = import('pkgconfig')
pkgg.generate(
  libraries : colorhug,
  requires : [ 'gio-2.0', 'gobject-2.0', 'gusb', 'colord' ],
  subdirs : 'colord-1/colorhug',
  version : meson.project_version(),
  name : 'colorhug',
  filebase : 'colorhug',
  description : 'ColorHug is a simple display hardware colorimeter',
)

libcolorhug_gir = gnome.generate_gir(colorhug,
  sources : [
    'ch-common.c',
    'ch-common.h',
    'ch-device.c',
    'ch-device.h',
    'ch-device-queue.c',
    'ch-device-queue.h',
    'ch-hash.c',
    'ch-hash.h',
  ],
  nsversion : '1.0',
  namespace : 'Colorhug',
  symbol_prefix : 'ch',
  identifier_prefix : 'Ch',
  export_packages : 'colorhug',
  c_args : [
      cargs,
    ],
  link_with : colorhug,
  dependencies : [
    gio,
    glib,
    lcms,
  ],
  includes : [
    'Gio-2.0',
    'GObject-2.0',
    'GUsb-1.0',
    libcolord_gir,
  ],
  install : true
)

if get_option('enable-tests')
  testdatadir = join_paths(meson.source_root(), 'data', 'tests')
  e = executable(
    'ch-self-test',
    sources : [
      'ch-self-test.c',
    ],
    include_directories : [
      include_directories('..'),
      include_directories('../colord'),
    root_incdir,
    ],
    dependencies : [
      gio,
      gusb,
      lcms,
    ],
    link_with : [
      colordprivate,
      colorhug,
    ],
    c_args : [
      cargs,
      '-DTESTDATADIR="' + testdatadir + '"',
    ],
  )
  test('colorhug-self-test', e)
endif
