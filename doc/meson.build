gnome.gtkdoc(meson.project_name(),
  namespace: PACKAGE_NAME,
  mode: 'sgml',
  main_sgml: meson.project_name() + '-docs.sgml',
  src_dir: join_paths(meson.source_root(), meson.project_name()),
  dependencies: declare_dependency(
    include_directories: default_includes,
    dependencies: LIBWNCK_DEPS,
    compile_args: libwnck_cflags,
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
