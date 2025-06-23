project('colord', 'c',
  version : '1.4.9',
  license : 'LGPL-2.1+',
  meson_version : '>=0.52.0',
  default_options : ['c_std=c99']
)

colord_version = meson.project_version()
varr = colord_version.split('.')
colord_major_version = varr[0]
colord_minor_version = varr[1]
colord_micro_version = varr[2]

conf = configuration_data()
conf.set('CD_MAJOR_VERSION_PRIVATE', colord_major_version)
conf.set('CD_MINOR_VERSION_PRIVATE', colord_minor_version)
conf.set('CD_MICRO_VERSION_PRIVATE', colord_micro_version)
conf.set_quoted('PACKAGE_VERSION', colord_version)

# libtool versioning - this applies to libcolord
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# - If interfaces have been changed or added, but binary compatibility
#   has been preserved, change:
#      CURRENT += 1
#      REVISION = 0
#      AGE += 1
# - If binary compatibility has been broken (eg removed or changed
#   interfaces), change:
#      CURRENT += 1
#      REVISION = 0
#      AGE = 0
# - If the interface is the same as the previous version, but bugs are
#   fixed, change:
#      REVISION += 1
lt_current = '2'
lt_revision = '5'
lt_age = '0'
lt_version = '@0@.@1@.@2@'.format(lt_current, lt_age, lt_revision)

# get supported warning flags
test_args = [
  '-fstack-protector-strong',
  '-Waggregate-return',
  '-Wunused',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wdeclaration-after-statement',
  '-Wempty-body',
  '-Wextra',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-discarded-qualifiers',
  '-Wno-cast-function-type',
  '-Wno-deprecated-declarations',
  '-Wno-enum-conversion',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpointer-arith',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
foreach arg: test_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach arg: test_link_args
  if cc.has_link_argument(arg)
    global_link_args += arg
  endif
endforeach
add_global_link_arguments(
  global_link_args,
  language: 'c'
)

gio = dependency('gio-2.0', version : '>= 2.45.8')
glib = dependency('glib-2.0', version : '>= 2.58')
gmodule = dependency('gmodule-2.0')
giounix = dependency('gio-unix-2.0', version : '>= 2.45.8')
lcms = dependency('lcms2', version : '>= 2.6')
sqlite = dependency('sqlite3')
gusb = dependency('gusb', version : '>= 0.2.7')
gudev = dependency('gudev-1.0')
libm = cc.find_library('m', required: false)
libudev = dependency('libudev')

if get_option('udev_rules')
  udev = dependency('udev')
endif

if get_option('systemd')
  systemd = dependency('systemd')
  libsystemd = dependency('libsystemd')
  conf.set('HAVE_SYSTEMD', '1')
  systemd_root_prefix = get_option('systemd_root_prefix')
  if systemd_root_prefix == ''
    systemduserunitdir = systemd.get_variable(pkgconfig: 'systemduserunitdir')
    systemdsystemunitdir = systemd.get_variable(pkgconfig: 'systemdsystemunitdir')
    tmpfilesdir = systemd.get_variable(pkgconfig: 'tmpfilesdir')
    sysusersdir = systemd.get_variable(pkgconfig: 'sysusersdir')
  else
    systemduserunitdir = systemd.get_variable(pkgconfig: 'systemduserunitdir', pkgconfig_define: ['rootprefix', systemd_root_prefix])
    systemdsystemunitdir = systemd.get_variable(pkgconfig: 'systemdsystemunitdir', pkgconfig_define: ['rootprefix', systemd_root_prefix])
    tmpfilesdir = systemd.get_variable(pkgconfig: 'tmpfilesdir', pkgconfig_define: ['rootprefix', systemd_root_prefix])
    sysusersdir = systemd.get_variable(pkgconfig: 'sysusersdir', pkgconfig_define: ['rootprefix', systemd_root_prefix])
  endif
endif

if cc.has_header('pwd.h')
  conf.set('HAVE_PWD_H', '1')
endif
if cc.has_header('syslog.h')
  conf.set('HAVE_SYSLOG_H', '1')
endif
if cc.has_header('unistd.h')
  conf.set('HAVE_UNISTD_H', '1')
endif
if cc.has_function('getuid', prefix : '#include<unistd.h>')
  conf.set('HAVE_GETUID', '1')
endif

if get_option('libcolordcompat')
  conf.set('BUILD_LIBCOLORDCOMPAT', '1')
endif
if get_option('session_example')
  gnome_desktop = dependency('gnome-desktop-3.0', version : '>= 0.2.7')
  colord_gtk = dependency('colord-gtk', version : '>= 0.1.24')
endif
if get_option('bash_completion')
  bash_completion = dependency('bash-completion', version : '>= 2.0')
endif

if get_option('sane')
  sane = dependency('sane-backends')
  dbus = dependency('dbus-1')
  conf.set('HAVE_SANE', '1')
endif

if get_option('argyllcms_sensor')
  spotread = find_program('spotread', required : false)
  if spotread.found() == false
    warning('spotread not found. Argyll sensor will not work unless argyll is installed')
  endif
  conf.set('HAVE_ARGYLLCMS_SENSOR', '1')
endif

if get_option('print_profiles')
  colprof = find_program('colprof')
  conf.set_quoted('TOOL_COLPROF', colprof.path())
endif

if get_option('vapi')
  vapigen = find_program('vapigen')
endif

if get_option('daemon')
conf.set_quoted('DAEMON_USER', get_option('daemon_user'))
if get_option('daemon_user') == 'root'
  warning('RUNNING THE DAEMON AS root IS NOT A GOOD IDEA, use -Ddaemon_user= to set user')
endif
endif

if get_option('pnp_ids') != ''
  conf.set_quoted('PNP_IDS', get_option('pnp_ids'))
endif

if get_option('daemon')
polkit = dependency('polkit-gobject-1', version : '>= 0.103')
if polkit.version().version_compare('>= 0.114')
  conf.set('POLKIT_HAS_AUTOPTR_MACROS', '1')
endif
endif

if meson.version().version_compare('>0.41.0')
  valgrind = dependency('valgrind', required: false)
else
  valgrind = dependency('valgrindXXX', required: false)
endif

if valgrind.found()
  conf.set('HAVE_VALGRIND', '1')
endif

gnome = import('gnome')
i18n = import('i18n')

add_project_arguments('-DCD_COMPILATION', language: 'c')

# Needed for realpath() and PATH_MAX
add_project_arguments('-D_XOPEN_SOURCE=700', language : 'c')
# Needed to opt-in to 64-bit time_t on glibc. We pass 64-bit pointers into
# gmtime_r. We have to set F_O_B here despite Meson defaulting to it because
# it doesn't get passed down to introspection otherwise.
add_project_arguments('-D_FILE_OFFSET_BITS=64', language : 'c')
add_project_arguments('-D_TIME_BITS=64', language : 'c')

prefix = get_option('prefix')

bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
datadir = join_paths(prefix, get_option('datadir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
localstatedir = join_paths(prefix, get_option('localstatedir'))
sysconfdir = join_paths(prefix, get_option('sysconfdir'))
mandir = join_paths(prefix, get_option('mandir'))
includedir = join_paths(prefix, get_option('includedir'))

conf.set_quoted('SYSCONFDIR', sysconfdir)
conf.set_quoted('BINDIR', bindir)
conf.set_quoted('LIBDIR', libdir)
conf.set_quoted('DATADIR', datadir)
conf.set_quoted('LIBEXECDIR', libexecdir)
conf.set_quoted('LOCALSTATEDIR', localstatedir)

cd_system_profiles_dir = join_paths(localstatedir,
                                    'lib', 'colord', 'icc')
conf.set_quoted('CD_SYSTEM_PROFILES_DIR', cd_system_profiles_dir)

conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('LOCALEDIR', get_option('localedir'))
configure_file(
  output : 'config.h',
  configuration : conf
)

root_incdir = include_directories('.')

subdir('lib')
subdir('po')

# this needs libcolord
subdir('client')
subdir('contrib')
if get_option('docs')
  subdir('doc')
endif

# this needs client/cd-create-profile
subdir('data')

# this needs data/profiles/*.icc
if get_option('man')
  subdir('man')
endif
if get_option('daemon')
  if get_option('udev_rules')
    subdir('rules')
  endif
  subdir('policy')
  subdir('src')
endif

meson.add_install_script('meson_post_install.sh',
                         localstatedir, get_option('daemon_user'))

install_data('org.freedesktop.colord.metainfo.xml',
  install_dir : join_paths(datadir, 'metainfo')
  )
