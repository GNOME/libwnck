project('libwnck',
  'c',
  version: '3.30.0',
  meson_version: '>= 0.47.999')

LIBWNCK_SOVERSION = 0

# libtool versioning for libwnck
# increment if the interface has additions, changes, removals.
LIBWNCK_CURRENT = 3

# increment any time the source changes; set to
# 0 if you increment CURRENT
LIBWNCK_REVISION = 0

# increment if any interfaces have been added; set to 0
# if any interfaces have been changed or removed. removal has
# precedence over adding, so set to 0 if both happened.
LIBWNCK_AGE = 3

MODULE_NAME = meson.project_name() + '-@0@'.format(LIBWNCK_CURRENT)
MODULE_VERSION = '@0@.@1@'.format(LIBWNCK_CURRENT, LIBWNCK_REVISION)
PACKAGE_NAME = MODULE_NAME + '.@0@'.format(LIBWNCK_REVISION)
LIBNAME = MODULE_NAME.split('lib')[1]

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
includedir = join_paths(prefix, get_option('includedir'))
localedir = join_paths(prefix, get_option('localedir'))

cc = meson.get_compiler('c')
default_includes = include_directories('.')

#####
# CFLAGS
# The same as the AX_COMPILER_FLAGS Autotools macro.

warning_cflags = [
  '-fno-strict-aliasing',
  '-Wall',
  '-Wextra',
  '-Wundef',
  '-Wnested-externs',
  '-Wwrite-strings',
  '-Wpointer-arith',
  '-Wmissing-declarations',
  '-Wmissing-prototypes',
  '-Wstrict-prototypes',
  '-Wredundant-decls',
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers',
  '-Wdeclaration-after-statement',
  '-Wformat=2',
  '-Wold-style-definition',
  '-Wcast-align',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wshadow',
  '-Winline',
  '-Wpacked',
  '-Wmissing-format-attribute',
  '-Wmissing-noreturn',
  '-Winit-self',
  '-Wredundant-decls',
  '-Wmissing-include-dirs',
  '-Wunused-but-set-variable',
  '-Warray-bounds',
  '-Wimplicit-function-declaration',
  '-Wreturn-type',
  '-Wswitch-enum',
  '-Wswitch-default',
  '-Wduplicated-cond',
  '-Wduplicated-branches',
  '-Wlogical-op',
  '-Wrestrict',
  '-Wnull-dereference',
  '-Wjump-misses-init',
  '-Wdouble-promotion'
]

supported_warning_cflags = cc.get_supported_arguments(warning_cflags)
add_global_arguments(supported_warning_cflags, language : 'c')
##### end CFLAGS

conf = configuration_data()
check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_UNISTD_H', 'unistd.h'],
]

foreach h: check_headers
  if cc.has_header(h.get(1))
    conf.set(h.get(0), 1)
  endif
endforeach

check_functions = [
  ['HAVE_DCGETTEXT', 'dcgettext', '#include<libintl.h>'],
]

foreach f: check_functions
  if cc.has_function(f.get(1), prefix : f.get(2))
    conf.set(f.get(0), 1)
  endif
endforeach

LIBWNCK_DEPS = [cc.find_library('m', required: true)]
STARTUP_NOTIFICATION_PACKAGE = 'libstartup-notification-1.0'
X11_PACKAGE = 'x11'
XRES_PACKAGE = 'xres'

pkg_deps = [
  ['cairo-xlib-xrender', [], false],
  ['glib-2.0', '>= 2.32', true],
  ['gobject-2.0', '>= 2.13.0', true],
  ['gtk+-3.0', '>= 3.22.0', true],
  [STARTUP_NOTIFICATION_PACKAGE, '>= 0.4', get_option('startup-notification').enabled()],
  [X11_PACKAGE, [], true],
  [XRES_PACKAGE, [], false],
]

foreach p: pkg_deps
  pkg = p.get(0)
  required = p.get(2)
  dep = dependency(pkg, version: p.get(1), required: required)
  LIBWNCK_DEPS += dep

  if not required and dep.found()
    conf.set('HAVE_' + pkg.to_upper().underscorify(), 1)
  endif
endforeach

conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_STRING', meson.project_name() + ' ' + meson.project_version())
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', PACKAGE_NAME)

configure_file(input: 'config.h.meson',
  output : 'config.h',
  configuration : conf)

pc_conf = configuration_data()
pc_conf.set('prefix', get_option('prefix'))
pc_conf.set('exec_prefix', '${prefix}')
pc_conf.set('libdir', '${exec_prefix}/' + get_option('libdir'))
pc_conf.set('includedir', '${prefix}/' + get_option('includedir'))
pc_conf.set('STARTUP_NOTIFICATION_PACKAGE', STARTUP_NOTIFICATION_PACKAGE)
pc_conf.set('X11_PACKAGE', X11_PACKAGE)
pc_conf.set('XRES_PACKAGE', XRES_PACKAGE)
pc_conf.set('VERSION', meson.project_version())

foreach pc: [PACKAGE_NAME, PACKAGE_NAME + '-uninstalled']
  install_path = []
  if not pc.contains('-uninstalled')
    install_path = join_paths(libdir, 'pkgconfig')
  endif
  configure_file(input: pc + '.pc.in',
    output: pc + '.pc',
    configuration: pc_conf,
    install_dir: install_path)
endforeach

subdir('libwnck')
subdir('po')

if get_option('enable-gtk-doc')
  subdir('doc')
endif
