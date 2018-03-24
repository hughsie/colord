con2 = configuration_data()
con2.set('DAEMON_USER', get_option('daemon_user'))
policy_in = configure_file(
  input : 'org.freedesktop.color.policy.in.in',
  output : 'org.freedesktop.color.policy.in',
  configuration : con2,
)

#newer polkit has the ITS rules included
if polkit.version().version_compare('>0.113')
  i18n.merge_file(
    input: policy_in,
    output: 'org.freedesktop.color.policy',
    install: true,
    install_dir: join_paths(datadir, 'polkit-1', 'actions') ,
    type: 'xml',
    po_dir: join_paths(meson.source_root(), 'po')
  )
#older polkit is missing ITS rules and will fail
else
  i18n.merge_file(
    input: policy_in,
    output: 'org.freedesktop.color.policy',
    install: true,
    install_dir: join_paths(datadir, 'polkit-1', 'actions') ,
    type: 'xml',
    data_dirs: join_paths(meson.source_root(), 'policy'),
    po_dir: join_paths(meson.source_root(), 'po')
  )
endif
