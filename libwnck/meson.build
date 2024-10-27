gnome = import('gnome')
pkgconfig = import('pkgconfig')

libwnck_includedir = join_paths(PACKAGE_NAME, meson.project_name())

version_data = configuration_data()
version_array = meson.project_version().split('.')

major_version = version_array[0].to_int()
minor_version = version_array[1]
micro_version = (version_array.length() == 3) ? version_array[2].to_int() : 0

if minor_version == 'alpha'
  minor_version = 97
elif minor_version == 'beta'
  minor_version = 98
elif minor_version == 'rc'
  minor_version = 99
else
  # Ensure that only numbers are expected in this case
  minor_version = minor_version.to_int()
endif

if minor_version >= 97
  major_version = major_version - 1
endif

version_data.set('WNCK_MAJOR_VERSION', major_version)
version_data.set('WNCK_MINOR_VERSION', minor_version)
version_data.set('WNCK_MICRO_VERSION', micro_version)

headers = [
  'libwnck.h',
  'pager.h',
  'application.h',
  'class-group.h',
  'screen.h',
  'selector.h',
  'tasklist.h',
  'misc.h',
  'resource.h',
  'util.h',
  'window.h',
  'window-action-menu.h',
  'wnck-handle.h',
  'wnck-macros.h',
  'workspace.h',
]

headers += configure_file(
  input: 'version.h.in',
  output: 'version.h',
  configuration: version_data)

sources = [
  'application.c',
  'class-group.c',
  'pager.c',
  'screen.c',
  'selector.c',
  'tasklist.c',
  'util.c',
  'window-action-menu.c',
  'window.c',
  'wnck-handle.c',
  'wnck-icon-cache.c',
  'wnck-image-menu-item.c',
  'wnck-resource-usage.c',
  'workspace.c',
  'xutils.c',
]

a11y_sources = [
  'pager-accessible.c',
  'pager-accessible.h',
  'pager-accessible-factory.c',
  'pager-accessible-factory.h',
  'workspace-accessible.c',
  'workspace-accessible.h',
  'workspace-accessible-factory.c',
  'workspace-accessible-factory.h',
]

enum_types = gnome.mkenums_simple('wnck-enum-types',
  sources : headers,
  decorator: 'WNCK_EXPORT',
  header_prefix: '#include <libwnck/wnck-macros.h>',
  install_header: true,
  install_dir: join_paths(includedir, libwnck_includedir)
)

resources = gnome.compile_resources(
  '@0@-resources'.format(meson.project_name()),
  'wnck.gresource.xml',
  source_dir: '.',
  c_name: meson.project_name()
)

libwnck_cflags = [
  '-DG_LOG_DOMAIN="Wnck"',
  '-DWNCK_I_KNOW_THIS_IS_UNSTABLE',
  '-DWNCK_LOCALEDIR="@0@"'.format(localedir),
  '-DWNCK_COMPILATION',
  '-DSN_API_NOT_YET_FROZEN=1'
]

if get_option('deprecation_flags')
  foreach domain: ['G', 'ATK', 'GDK', 'GDK_PIXBUF', 'GTK', 'WNCK']
    libwnck_cflags += '-D@0@_DISABLE_DEPRECATED'.format(domain)
  endforeach
endif

libwnck_dep = declare_dependency(
  include_directories: default_includes,
  dependencies: LIBWNCK_DEPS,
  compile_args: libwnck_cflags,
  sources: headers + [enum_types[1]],
)

libwnck_lib = shared_library(LIBNAME,
  dependencies: libwnck_dep,
  sources: sources + a11y_sources + enum_types + resources,
  version: '@0@.@1@.@2@'.format(LIBWNCK_SOVERSION, LIBWNCK_CURRENT, LIBWNCK_REVISION),
  soversion: LIBWNCK_SOVERSION,
  gnu_symbol_visibility: 'hidden',
  install: true,
)

libwnck_requires_private = [
  STARTUP_NOTIFICATION_PACKAGE,
  X11_PACKAGE,
  'pango',
  'cairo',
]

if conf.has('HAVE_' + XRES_PACKAGE.to_upper().underscorify())
  libwnck_requires_private += XRES_PACKAGE
endif

pkgconfig.generate(
  libraries: libwnck_lib,
  name: 'libwnck',
  description: 'Window Navigator Construction Kit library',
  version: meson.project_version(),
  filebase: PACKAGE_NAME,
  subdirs: PACKAGE_NAME,
  requires: [
    'glib-2.0',
    'gobject-2.0',
    'gtk+-3.0'
  ],
  requires_private: libwnck_requires_private,
  variables: [
    'exec_prefix=${prefix}'
  ]
)

introspection = get_option('introspection')
if not introspection.disabled()
  find_program('g-ir-scanner', required: introspection.enabled())
  gnome.generate_gir(libwnck_lib,
    sources: headers + sources + enum_types,
    namespace: 'Wnck',
    nsversion: MODULE_VERSION,
    export_packages: PACKAGE_NAME,
    includes: ['GObject-2.0', 'GdkPixbuf-2.0', 'Gtk-3.0'],
    extra_args: '--c-include=@0@/@0@.h'.format(meson.project_name()),
    install: true
  )
endif

install_headers(headers, subdir: libwnck_includedir)

progs = [
  'wnckprop',
]

test_progs = [
  'test-wnck',
  'test-tasklist',
  'test-selector',
  'test-pager',
  'test-urgent',
  'test-shutdown',
  'test-icon-cache',
]

foreach prog: progs + test_progs
  executable(prog, [prog + '.c'],
    include_directories: default_includes,
    dependencies: libwnck_dep,
    link_with: libwnck_lib,
    install: progs.contains(prog) and get_option('install_tools'),
    install_dir: bindir)
endforeach
