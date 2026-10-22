project(
  'plainroot-validation', ['c', 'cpp'],
  version: '0.1.0',
  license: 'LGPL-3.0-or-later',
  license_files: ['LICENSE', 'COPYING'],
  meson_version: '>= 1.3.0',
  default_options: [
    'warning_level=3',
    'c_std=c11',
    'cpp_std=c++23',
    'default_library=shared',
  ],
)

public_inc = include_directories('include')

validation_library = library(
  'plainroot-validation',
  'src/report.cpp',
  include_directories: public_inc,
  cpp_args: ['-DPLAINROOT_VALIDATION_BUILD', '-fvisibility-inlines-hidden'],
  gnu_symbol_visibility: 'hidden',
  version: meson.project_version(),
  soversion: '0',
  link_args: ['-Wl,--version-script=' +
    (meson.current_source_dir() / 'plainroot-validation.map')],
  link_depends: files('plainroot-validation.map'),
  install: true,
)

install_headers(
  'include/plainroot/validation/export.h',
  'include/plainroot/validation/validation.h',
  'include/plainroot/validation/validation.hpp',
  subdir: 'plainroot/validation',
)

pkg = import('pkgconfig')
pkg.generate(
  validation_library,
  name: 'plainroot-validation',
  description: 'Independent structured validation results for PlainrootDE',
  version: meson.project_version(),
  subdirs: '.',
)

executable(
  'plainroot-validation-cli',
  'examples/validation_cli.cpp',
  include_directories: public_inc,
  link_with: validation_library,
  install: false,
)
