docbook2man = find_program('docbook2man')
custom_target('colormgr-man',
  input : 'colormgr.sgml',
  output : 'colormgr.1',
  command : [
    docbook2man, '@INPUT@',
    '--output', meson.current_build_dir(),
  ],
  install : true,
  install_dir : join_paths(mandir, 'man1'),
)
custom_target('cd-create-profile-man',
  input : 'cd-create-profile.sgml',
  output : 'cd-create-profile.1',
  command : [
    docbook2man, '@INPUT@',
    '--output', meson.current_build_dir(),
  ],
  install : true,
  install_dir : join_paths(mandir, 'man1'),
)
custom_target('cd-fix-profile-man',
  input : 'cd-fix-profile.sgml',
  output : 'cd-fix-profile.1',
  command : [
    docbook2man, '@INPUT@',
    '--output', meson.current_build_dir(),
  ],
  install : true,
  install_dir : join_paths(mandir, 'man1'),
)
custom_target('cd-it8-man',
  input : 'cd-it8.sgml',
  output : 'cd-it8.1',
  command : [
    docbook2man, '@INPUT@',
    '--output', meson.current_build_dir(),
  ],
  install : true,
  install_dir : join_paths(mandir, 'man1'),
)
