con2 = configuration_data()
con2.set('DAEMON_USER', get_option('daemon_user'))
sensor_rules_in = configure_file(
  input : '69-cd-sensors.rules.in',
  output : '69-cd-sensors.rules',
  configuration : con2,
)
install_data([
    sensor_rules_in,
    '95-cd-devices.rules',
  ],
  install_dir : join_paths(udev.get_pkgconfig_variable('udevdir'), 'rules.d')
)
