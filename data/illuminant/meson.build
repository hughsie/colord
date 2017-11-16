generated_spectra = [
  'CIE-A',
  'CIE-B',
  'CIE-C',
  'CIE-D50',
  'CIE-D55',
  'CIE-D65',
  'CIE-D93',
  'CIE-E',
  'CIE-F10',
  'CIE-F11',
  'CIE-F12',
  'CIE-F1',
  'CIE-F2',
  'CIE-F3',
  'CIE-F4',
  'CIE-F5',
  'CIE-F6',
  'CIE-F7',
  'CIE-F8',
  'CIE-F9',
]
foreach arg: generated_spectra
  custom_target(arg,
    input: arg + '.csv',
    output: arg + '.sp',
    command: [ cd_idt8, 'create-sp', '@OUTPUT@', '@INPUT@', '100.0' ],
    install: true,
    install_dir: join_paths(datadir, 'colord', 'illuminant')
  )
endforeach
