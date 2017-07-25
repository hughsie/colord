subdir('cmf')
subdir('figures')
subdir('illuminant')
subdir('profiles')
subdir('ref')
subdir('tests')
subdir('ti1')

install_data('colord.conf',
  install_dir: systemd.get_pkgconfig_variable('tmpfilesdir')
)

if get_option('enable-bash-completion')
  install_data('colormgr',
    install_dir: bash_completion.get_pkgconfig_variable('completionsdir')
  )
endif

con2 = configuration_data()
con2.set('servicedir',
         join_paths(get_option('prefix'),
                    get_option('libexecdir')))
con2.set('daemon_user', get_option('with-daemon-user'))

# replace @servicedir@ and @daemon_user@
configure_file(
  input : 'colord.service.in',
  output : 'colord.service',
  configuration : con2,
  install: true,
  install_dir: systemd.get_pkgconfig_variable('systemdsystemunitdir'),
)

# replace @servicedir@ and @daemon_user@
configure_file(
  input : 'org.freedesktop.ColorManager.service.in',
  output : 'org.freedesktop.ColorManager.service',
  configuration : con2,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'dbus-1', 'system-services') ,
)

# replace @daemon_user@
configure_file(
  input : 'org.freedesktop.ColorManager.conf.in',
  output : 'org.freedesktop.ColorManager.conf',
  configuration : con2,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'dbus-1', 'system.d') ,
)
