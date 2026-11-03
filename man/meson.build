python3 = find_program('python3')
generate_man = files('generate-man.py')

foreach man_page : ['colormgr', 'cd-create-profile', 'cd-fix-profile', 'cd-it8']
  custom_target(man_page + '-man',
    input : man_page + '.md',
    output : man_page + '.1',
    command : [
      python3, generate_man,
      '@INPUT@',
      '-o', '@OUTPUT@',
      '--replace', 'PACKAGE_VERSION', meson.project_version(),
    ],
    install : true,
    install_dir : join_paths(mandir, 'man1'),
  )
endforeach
