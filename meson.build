project('libwnck',
  'c',
  version: '43.1',
  meson_version: '>= 1.1.0')

LIBWNCK_SOVERSION = 0

# libtool versioning for libwnck
# increment if the interface has additions, changes, removals.
LIBWNCK_CURRENT = 4

# increment any time the source changes; set to
# 0 if you increment CURRENT
LIBWNCK_REVISION = 0

# increment if any interfaces have been added; set to 0
# if any interfaces have been changed or removed. removal has
# precedence over adding, so set to 0 if both happened.
LIBWNCK_AGE = 0

MODULE_NAME = meson.project_name() + '-@0@'.format(LIBWNCK_CURRENT)
MODULE_VERSION = '@0@.@1@'.format(LIBWNCK_CURRENT, LIBWNCK_REVISION)
PACKAGE_NAME = MODULE_NAME + '.@0@'.format(LIBWNCK_REVISION)
LIBNAME = MODULE_NAME.split('lib')[1]

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
includedir = join_paths(prefix, get_option('includedir'))
localedir = join_paths(prefix, get_option('localedir'))

default_includes = include_directories('.')
conf = configuration_data()
cc = meson.get_compiler('c')

LIBWNCK_DEPS = [cc.find_library('m', required: true)]
STARTUP_NOTIFICATION_PACKAGE = 'libstartup-notification-1.0'
X11_PACKAGE = 'x11'
XRES_PACKAGE = 'xres'

pkg_deps = [
  {'name': 'cairo-xlib-xrender', 'required': false},
  {'name': 'glib-2.0', 'version': '>= 2.44' },
  {'name': 'gobject-2.0', 'version': '>= 2.13.0' },
  {'name': 'gtk+-3.0', 'version': '>= 3.22.0' },
  {'name': STARTUP_NOTIFICATION_PACKAGE, 'version': '>= 0.4' },
  {'name': X11_PACKAGE },
  {'name': XRES_PACKAGE, 'version': '>= 1.2', 'required': false},
  {'name': 'xi', 'required': true},
]

foreach p: pkg_deps
  pkg = p.get('name')
  required = p.get('required', true)
  dep = dependency(pkg, version: p.get('version', []), required: required)
  LIBWNCK_DEPS += dep

  if dep.found()
    conf.set('HAVE_' + pkg.to_upper().underscorify(), 1)
  endif
endforeach

conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_STRING', meson.project_name() + ' ' + meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', PACKAGE_NAME)
conf.set_quoted('VERSION', meson.project_version())

configure_file(input: 'config.h.meson',
  output : 'config.h',
  configuration : conf)

subdir('libwnck')
subdir('po')

if get_option('gtk_doc')
  subdir('doc')
endif
