gnome.gtkdoc(meson.project_name(),
  namespace: PACKAGE_NAME,
  mode: 'sgml',
  main_sgml: meson.project_name() + '-docs.sgml',
  src_dir: include_directories('../libwnck'),
  dependencies: declare_dependency(
    dependencies: libwnck_dep,
    link_with: libwnck_lib
  ),
  gobject_typesfile: meson.project_name() + '.types',
  ignore_headers: [
    'pager-accessible.h',
    'pager-accessible-factory.h',
    'wnck-enum-types.h',
    'wnck-icon-cache-private.h',
    'wnck-image-menu-item-private.h',
    'workspace-accessible.h',
    'workspace-accessible-factory.h'
  ],
  scan_args: [
    '--rebuild-types',
    '--rebuild-sections'
  ],
  install: true,
  module_version: MODULE_VERSION,
)
