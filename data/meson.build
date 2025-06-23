subdir('cmf')
subdir('figures')
subdir('illuminant')
subdir('profiles')
subdir('ref')
subdir('tests')
subdir('ti1')

if get_option('bash_completion')
  install_data('colormgr',
    install_dir: bash_completion.get_pkgconfig_variable('completionsdir')
  )
endif

con2 = configuration_data()
con2.set('localstatedir', localstatedir)
con2.set('servicedir', libexecdir)
con2.set('daemon_user', get_option('daemon_user'))

# replace @servicedir@, @daemon_user@ and @localstatedir@
if get_option('systemd')
  configure_file(
    input : 'colord.service.in',
    output : 'colord.service',
    configuration : con2,
    install: true,
    install_dir: systemdsystemunitdir,
  )

  configure_file(
    input : 'colord.conf.in',
    output : 'colord.conf',
    configuration : con2,
    install: true,
    install_dir: tmpfilesdir,
  )

  if get_option('daemon_user') != 'root'
    configure_file(
      input : 'colord.sysusers.conf.in',
      output : 'colord-sysusers.conf',
      configuration : con2,
      install: true,
      install_dir: sysusersdir,
    )
  endif
endif

# replace @servicedir@ and @daemon_user@
configure_file(
  input : 'org.freedesktop.ColorManager.service.in',
  output : 'org.freedesktop.ColorManager.service',
  configuration : con2,
  install: true,
  install_dir: join_paths(datadir, 'dbus-1', 'system-services') ,
)

# replace @daemon_user@
configure_file(
  input : 'org.freedesktop.ColorManager.conf.in',
  output : 'org.freedesktop.ColorManager.conf',
  configuration : con2,
  install: true,
  install_dir: join_paths(datadir, 'dbus-1', 'system.d') ,
)
