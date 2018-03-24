icc_display_profiles = [
  'AdobeRGB1998',
  'AppleRGB',
  'BestRGB',
  'BetaRGB',
  'Bluish',
  'BruceRGB',
  'CIE-RGB',
  'ColorMatchRGB',
  'Crayons',
  'DonRGB4',
  'ECI-RGBv1',
  'ECI-RGBv2',
  'EktaSpacePS5',
  'Gamma5000K',
  'Gamma5500K',
  'Gamma6500K',
  'NTSC-RGB',
  'PAL-RGB',
  'ProPhotoRGB',
  'Rec709',
  'SMPTE-C-RGB',
  'sRGB',
  'SwappedRedAndGreen',
  'WideGamutRGB',
  'x11-colors',
]

icc_print_profiles = [
  'FOGRA27L_coated',
  'FOGRA28L_webcoated',
  'FOGRA29L_uncoated',
  'FOGRA30L_uncoated_yellowish',
  'FOGRA39L_coated',
  'FOGRA40L_SC_paper',
  'FOGRA45L_lwc',
  'FOGRA47L_uncoated',
  'GRACoL_TR006_coated',
  'IFRA26S_2004_newsprint',
  'SNAP_TR002_newsprint',
  'SWOP_TR003_coated_3',
  'SWOP_TR005_coated_5',
]

icc_profiles = icc_display_profiles
if get_option('print_profiles')
  icc_profiles += icc_print_profiles
endif

generated_iccs = []
foreach arg: icc_profiles
  xml_i18n = i18n.merge_file(
    input: arg + '.iccprofile.xml',
    output: arg + '.iccprofile.xml',
    type: 'xml',
    data_dirs: join_paths(meson.source_root(), 'data', 'profiles'),
    po_dir: join_paths(meson.source_root(), 'po')
  )
  generated_icc = custom_target(arg + '.icc',
    input: xml_i18n,
    output: arg + '.icc',
    command: [ cd_create_profile, '--output=@OUTPUT@', '@INPUT@' ],
    install: true,
    install_dir: join_paths(datadir, 'color', 'icc', 'colord'),
  )
  generated_iccs += generated_icc
endforeach
