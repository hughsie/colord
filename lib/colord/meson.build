cargs = [
  '-DG_LOG_DOMAIN="libcolord"',
]

con2 = configuration_data()
con2.set('CD_MAJOR_VERSION_PRIVATE', colord_major_version)
con2.set('CD_MINOR_VERSION_PRIVATE', colord_minor_version)
con2.set('CD_MICRO_VERSION_PRIVATE', colord_micro_version)

colord_version_h = configure_file(
  input : 'cd-version.h.in',
  output : 'cd-version.h',
  configuration : con2
)

install_headers(
  'colord.h',
  'colord-private.h',
  subdir : 'colord-1',
)

colord_incdir = include_directories('.')

install_headers([
    'cd-buffer.h',
    'cd-client.h',
    'cd-client-sync.h',
    'cd-color.h',
    'cd-device.h',
    'cd-device-sync.h',
    'cd-dom.h',
    'cd-edid.h',
    'cd-enum.h',
    'cd-icc.h',
    'cd-icc-store.h',
    'cd-icc-utils.h',
    'cd-interp-akima.h',
    'cd-interp-linear.h',
    'cd-interp.h',
    'cd-it8.h',
    'cd-it8-utils.h',
    'cd-math.h',
    'cd-profile.h',
    'cd-profile-sync.h',
    'cd-quirk.h',
    'cd-sensor.h',
    'cd-sensor-sync.h',
    'cd-spectrum.h',
    'cd-transform.h',
    colord_version_h,
  ],
  subdir : 'colord-1/colord',
)

shared_src = [
  'cd-buffer.c',
  'cd-color.c',
  'cd-context-lcms.c',
  'cd-dom.c',
  'cd-edid.c',
  'cd-enum.c',
  'cd-icc.c',
  'cd-icc-store.c',
  'cd-icc-utils.c',
  'cd-interp-akima.c',
  'cd-interp.c',
  'cd-interp-linear.c',
  'cd-it8.c',
  'cd-it8-utils.c',
  'cd-math.c',
  'cd-quirk.c',
  'cd-spectrum.c',
  'cd-transform.c',
]

mapfile = 'colord.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
colordprivate = shared_library(
  'colordprivate',
  sources : shared_src,
  soversion : lt_current,
  version : lt_version,
  dependencies : [
    giounix,
    gudev,
    lcms,
    libm,
    libudev,
  ],
  c_args : [
      cargs,
      '-DLOCALSTATEDIR="' + localstatedir + '"',
    ],
  include_directories : [
      root_incdir,
      lib_incdir,
  ],
  link_args : vflag,
  link_depends : mapfile,
  install : true
)
colord = shared_library(
  'colord',
  sources : [
    'cd-client.c',
    'cd-client-sync.c',
    'cd-device.c',
    'cd-device-sync.c',
    'cd-profile.c',
    'cd-profile-sync.c',
    'cd-sensor.c',
    'cd-sensor-sync.c',
    shared_src,
  ],
  soversion : lt_current,
  version : lt_version,
  dependencies : [
    giounix,
    gudev,
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
      root_incdir,
      lib_incdir,
  ],
  link_args : vflag,
  link_depends : mapfile,
  install : true
)

pkgg = import('pkgconfig')
pkgg.generate(
  libraries : colord,
  requires : [ 'gio-2.0', 'gobject-2.0' ],
  subdirs : 'colord-1',
  version : meson.project_version(),
  name : 'colord',
  filebase : 'colord',
  description : 'colord is a system daemon for managing color devices',
)

if get_option('introspection')
libcolord_girtarget = gnome.generate_gir(colord,
  sources : [
    'cd-client.c',
    'cd-client.h',
    'cd-client-sync.c',
    'cd-client-sync.h',
    'cd-color.c',
    'cd-color.h',
    'cd-edid.c',
    'cd-edid.h',
    'cd-device.c',
    'cd-device.h',
    'cd-device-sync.c',
    'cd-device-sync.h',
    'cd-enum.c',
    'cd-enum.h',
    'cd-icc.c',
    'cd-icc.h',
    'cd-it8.c',
    'cd-it8.h',
    'cd-math.c',
    'cd-math.h',
    'cd-profile.c',
    'cd-profile.h',
    'cd-profile-sync.c',
    'cd-profile-sync.h',
    'cd-sensor.c',
    'cd-sensor.h',
    'cd-sensor-sync.c',
    'cd-sensor-sync.h',
    'cd-spectrum.c',
    'cd-spectrum.h',
  ],
  nsversion : '1.0',
  namespace : 'Colord',
  symbol_prefix : 'cd',
  identifier_prefix : 'Cd',
  export_packages : 'colord',
  extra_args : [
    '--c-include=colord.h',
    '-DCD_COMPILATION',
    ],
  link_with : colordprivate,
  dependencies : [
    gio,
    glib,
    lcms,
  ],
  includes : [
    'Gio-2.0',
    'GObject-2.0',
  ],
  install : true
)
libcolord_gir = libcolord_girtarget[0]
libcolord_typelib = libcolord_girtarget[1]
endif

if get_option('vapi')
gnome.generate_vapi('colord',
  sources: libcolord_girtarget[0],
  packages: ['gio-2.0'],
  install: true,
)
endif

if get_option('installed_tests')
con2 = configuration_data()
con2.set('installedtestsdir',
         join_paths(libexecdir, 'installed-tests', 'colord'))
configure_file(
  input : 'colord-daemon.test.in',
  output : 'colord-daemon.test',
  configuration : con2,
  install: true,
  install_dir: join_paths('share', 'installed-tests', 'colord'),
)
configure_file(
  input : 'colord-private.test.in',
  output : 'colord-private.test',
  configuration : con2,
  install: true,
  install_dir: join_paths('share', 'installed-tests', 'colord'),
)
endif

if get_option('tests')
  testdatadir = environment({'TESTDATADIR' : join_paths(meson.source_root(), 'data', 'tests')})

  e = executable(
    'colord-test-private',
    sources : [
      'cd-test-private.c',
      'cd-test-shared.h',
      'cd-test-shared.c',
    ],
    include_directories : [
      root_incdir,
      lib_incdir,
    ],
    dependencies : [
      gio,
      lcms,
    ],
    link_with : colordprivate,
    install : get_option('installed_tests'),
    install_dir : join_paths(libexecdir, 'installed-tests', 'colord'),
  )
  test('colord-test-private', e, env : testdatadir)
  e = executable(
    'colord-test-daemon',
    sources : [
      'cd-test-daemon.c',
      'cd-test-shared.h',
      'cd-test-shared.c',
    ],
    include_directories : [
      root_incdir,
      lib_incdir,
    ],
    dependencies : [
      gio,
      lcms,
    ],
    link_with : colord,
    install : get_option('installed_tests'),
    install_dir : join_paths(libexecdir, 'installed-tests', 'colord'),
  )
  test('colord-test-daemon', e, env : testdatadir)
endif
