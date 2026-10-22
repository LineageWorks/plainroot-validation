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
re2_dep = dependency('re2', method: 'pkg-config')

validation_library = library(
  'plainroot-validation',
  'src/report.cpp',
  'src/numeric.cpp',
  'src/text.cpp',
  'src/regex.cpp',
  include_directories: public_inc,
  dependencies: [re2_dep],
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
  requires_private: [re2_dep],
)

validation_cli = executable(
  'plainroot-validation-cli',
  'examples/validation_cli.cpp',
  include_directories: public_inc,
  link_with: validation_library,
  install: true,
)

cli_test_script = find_program('tests/validation_cli_test.sh')
test(
  'validation-cli',
  cli_test_script,
  args: [validation_cli.full_path()],
)

validation_cpp_test = executable(
  'plainroot-validation-cpp-test',
  'tests/numeric_test.cpp',
  include_directories: public_inc,
  link_with: validation_library,
  install: false,
)
test('validation-cpp', validation_cpp_test)

validation_text_regex_cpp_test = executable(
  'plainroot-validation-text-regex-cpp-test',
  'tests/text_regex_test.cpp',
  include_directories: public_inc,
  link_with: validation_library,
  install: false,
)
test('validation-text-regex-cpp', validation_text_regex_cpp_test)

validation_composition_test = executable(
  'plainroot-validation-composition-test',
  'tests/composition_test.cpp',
  include_directories: public_inc,
  link_with: validation_library,
  install: false,
)
test('validation-composition', validation_composition_test)

validation_c_api_test = executable(
  'plainroot-validation-c-api-test',
  'tests/c_api_test.c',
  include_directories: public_inc,
  link_with: validation_library,
  install: false,
)
test('validation-c-api', validation_c_api_test)

validation_text_regex_c_api_test = executable(
  'plainroot-validation-text-regex-c-api-test',
  'tests/text_regex_c_api_test.c',
  include_directories: public_inc,
  link_with: validation_library,
  install: false,
)
test('validation-text-regex-c-api', validation_text_regex_c_api_test)

lua_adapter_feature = get_option('lua_adapter')
lua_dep = dependency(
  'lua',
  version: '>=5.4,<5.5',
  required: lua_adapter_feature,
)

if lua_dep.found()
  lua_validation_module = shared_module(
    'plainroot_validation',
    'src/lua_module.cpp',
    include_directories: public_inc,
    dependencies: [lua_dep],
    link_with: validation_library,
    name_prefix: '',
    install: true,
    install_dir: get_option('libdir') / 'lua' / '5.4',
  )

  lua_test_harness = executable(
    'plainroot-validation-lua-test',
    'tests/lua_test_harness.cpp',
    dependencies: [lua_dep],
    install: false,
  )
  test(
    'validation-lua-adapter',
    lua_test_harness,
    args: [
      lua_validation_module.full_path(),
      meson.current_source_dir() / 'tests/lua_adapter_test.lua',
    ],
    depends: lua_validation_module,
  )
endif
