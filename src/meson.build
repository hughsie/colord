src_incdir = include_directories('.')

subdir('plugins')
subdir('sensors')

cargs = ['-DG_LOG_DOMAIN="Cd"']

install_data([
    'org.freedesktop.ColorManager.xml',
    'org.freedesktop.ColorManager.Device.xml',
    'org.freedesktop.ColorManager.Sensor.xml',
    'org.freedesktop.ColorManager.Profile.xml',
  ],
  install_dir : join_paths(datadir, 'dbus-1', 'interfaces')
)

resources_src = gnome.compile_resources(
  'colord-resources',
  'colord.gresource.xml',
  source_dir : [
    '.',
    '../data/profiles',
  ],
  c_name : 'cd',
  dependencies : generated_iccs,
)

colord_extra_deps = []
if get_option('systemd')
  colord_extra_deps += libsystemd
endif

executable(
  'colord',
  resources_src,
  sources : [
    'cd-common.c',
    'cd-debug.c',
    'cd-device-array.c',
    'cd-device.c',
    'cd-device-db.c',
    'cd-inhibit.c',
    'cd-inhibit.h',
    'cd-main.c',
    'cd-mapping-db.c',
    'cd-plugin.c',
    'cd-plugin.h',
    'cd-profile-array.c',
    'cd-profile.c',
    'cd-profile-db.c',
    'cd-sensor.c',
    'cd-sensor-client.c',
  ],
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
  ],
  dependencies : [
    colord_extra_deps,
    giounix,
    gmodule,
    gudev,
    gusb,
    lcms,
    polkit,
    sqlite,
  ],
  link_with : colordprivate,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : libexecdir
)

if get_option('tests')
  e = executable(
    'cd-self-test',
    resources_src,
    sources : [
      'cd-common.c',
      'cd-device-array.c',
      'cd-device.c',
      'cd-device-db.c',
      'cd-inhibit.c',
      'cd-mapping-db.c',
      'cd-profile-array.c',
      'cd-profile-db.c',
      'cd-profile.c',
      'cd-self-test.c',
    ],
    include_directories : [
      colord_incdir,
      lib_incdir,
      root_incdir,
    ],
    dependencies : [
      giounix,
      gmodule,
      gudev,
      gusb,
      lcms,
      polkit,
      sqlite,
    ],
    link_with : [
      colordprivate,
    ],
    c_args : [
      cargs,
    ],
  )
  test('cd-self-test', e)
endif
