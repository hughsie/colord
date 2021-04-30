cargs = [
  '-DG_LOG_DOMAIN="libcolorhug"',
]

ch_version_h_conf = configuration_data()
ch_version_h_conf.set('CD_MAJOR_VERSION', colord_major_version)
ch_version_h_conf.set('CD_MINOR_VERSION', colord_minor_version)
ch_version_h_conf.set('CD_MICRO_VERSION', colord_micro_version)

colorhug_version_h = configure_file(
  input : 'ch-version.h.in',
  output : 'ch-version.h',
  configuration : ch_version_h_conf,
  install : true,
  install_dir: join_paths(includedir, 'colord-1', 'colorhug'),
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

if get_option('introspection')
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
  extra_args : [
    '--c-include=colorhug.h',
    '-DCD_COMPILATION',
  ],
  install : true
)
endif

if get_option('tests')
  e = executable(
    'ch-self-test',
    sources : [
      'ch-self-test.c',
    ],
    include_directories : [
      colord_incdir,
      lib_incdir,
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
  )
  test('colorhug-self-test', e)
endif
