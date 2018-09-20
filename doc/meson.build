gnome.gtkdoc(meson.project_name(),
  namespace: PACKAGE_NAME,
  mode: 'sgml',
  main_sgml: meson.project_name() + '-docs.sgml',
  src_dir: join_paths(meson.source_root(), meson.project_name()),
  dependencies: declare_dependency(
    dependencies: libwnck_dep,
    link_with: libwnck_lib
  ),
  gobject_typesfile: meson.project_name() + '.types',
  scan_args: [
    '--rebuild-types',
    '--ignore-headers=' + ' '.join(['wnck-enum-types.h', 'version.h']),
    '--deprecated-guards="WNCK_DISABLE_DEPRECATED"'
  ],
  install: true,
  module_version: MODULE_VERSION,
)
