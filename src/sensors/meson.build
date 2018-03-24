subdir('argyll')
subdir('colorhug')
subdir('dtp94')
subdir('dummy')
subdir('huey')
subdir('huey2')
subdir('munki')

cargs = ['-DG_LOG_DOMAIN="Cd"']

if get_option('reverse')
  executable(
    'cd-parse-beagle',
    sources : [
      'cd-parse-beagle.c',
    ],
    include_directories : [
    root_incdir,
    ],
    dependencies : [
      gio,
    ],
    link_with : colordprivate,
    c_args : [
      cargs,
    ],
  )
endif
