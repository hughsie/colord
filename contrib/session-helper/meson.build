cargs = ['-DG_LOG_DOMAIN="Cd"']

install_data('org.freedesktop.ColorHelper.xml',
  install_dir : join_paths(datadir, 'dbus-1', 'interfaces')
)

install_headers(
  'cd-session.h',
  subdir : 'colord-1/colord-session',
)

con2 = configuration_data()
con2.set('servicedir', libexecdir)

# replace @servicedir@
if get_option('systemd')
  configure_file(
    input : 'colord-session.service.in',
    output : 'colord-session.service',
    configuration : con2,
    install: true,
    install_dir: systemduserunitdir,
  )
endif

# replace @servicedir@
configure_file(
  input : 'org.freedesktop.ColorHelper.service.in',
  output : 'org.freedesktop.ColorHelper.service',
  configuration : con2,
  install: true,
  install_dir: join_paths(datadir, 'dbus-1', 'services') ,
)

executable(
  'colord-session',
  sources : [
    'cd-debug.c',
    'cd-debug.h',
    'cd-main.c',
    'cd-session.h',
    'cd-state.c',
    'cd-state.h',
  ],
  include_directories : [
    colord_incdir,
    lib_incdir,
    root_incdir,
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
  install_dir : libexecdir
)

install_data('org.freedesktop.ColorHelper.gschema.xml',
             install_dir : 'share/glib-2.0/schemas')

if get_option('session_example')
  executable(
    'colord-session-example',
    sources : [
      'cd-example.c',
    ],
    include_directories : [
    root_incdir,
      colord_incdir,
      lib_incdir,
      root_incdir,
    ],
    dependencies : [
      gio,
      lcms,
      libm,
      colord_gtk,
      gnome_desktop,
    ],
    link_with : colord,
    c_args : [
      cargs,
    ],
  )
endif
