gnome = import('gnome')

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
  'util.h',
  'window.h',
  'window-action-menu.h',
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
  'wnck-handle-private.h',
  'wnck-handle.c',
  'wnck-image-menu-item-private.h',
  'wnck-image-menu-item.c',
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

mapfile = MODULE_NAME + '.map'
libwnck_ldflags = [
  '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile),
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
  link_args: libwnck_ldflags,
)

libwnck_lib = shared_library(LIBNAME,
  dependencies: libwnck_dep,
  sources: sources + a11y_sources + enum_types + resources,
  version: '@0@.@1@.@2@'.format(LIBWNCK_SOVERSION, LIBWNCK_CURRENT, LIBWNCK_REVISION),
  soversion: LIBWNCK_SOVERSION,
  link_depends: mapfile,
  install: true,
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

if not get_option('deprecation_flags')
  progs += 'wnck-urgency-monitor'
endif

test_progs = [
  'test-wnck',
  'test-tasklist',
  'test-selector',
  'test-pager',
  'test-urgent',
  'test-shutdown',
]

foreach prog: progs + test_progs
  executable(prog, [prog + '.c'],
    include_directories: default_includes,
    dependencies: libwnck_dep,
    link_with: libwnck_lib,
    install: progs.contains(prog) and get_option('install_tools'),
    install_dir: bindir)
endforeach
