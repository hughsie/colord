foreach arg: [ 'CIE1964-10deg-XYZ', 'CIE1931-2deg-XYZ' ]
  custom_target(arg,
    input: arg + '.csv',
    output: arg + '.cmf',
    command: [ cd_idt8, 'create-cmf', '@OUTPUT@', '@INPUT@', '1.0' ],
    install: true,
    install_dir: join_paths(datadir, 'colord', 'cmf')
  )
endforeach
