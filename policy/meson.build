con2 = configuration_data()
con2.set('DAEMON_USER', get_option('with-daemon-user'))
policy_in = configure_file(
  input : 'org.freedesktop.color.policy.in.in',
  output : 'org.freedesktop.color.policy.in',
  configuration : con2,
)
i18n.merge_file(
  input: policy_in,
  output: 'org.freedesktop.color.policy',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'polkit-1', 'actions') ,
  type: 'xml',
  po_dir: join_paths(meson.source_root(), 'po')
)
