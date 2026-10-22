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

nm_program = find_program('nm', required: host_machine.system() == 'linux')
readelf_program = find_program(
  'readelf', required: host_machine.system() == 'linux')
if nm_program.found() and readelf_program.found()
  abi_test_script = find_program('tests/abi_test.sh')
  test(
    'validation-abi-and-dependencies',
    abi_test_script,
    args: [
      validation_library.full_path(),
      meson.current_source_dir() / 'include/plainroot/validation/validation.h',
      nm_program.full_path(),
      readelf_program.full_path(),
      meson.current_source_dir() / 'abi/PLAINROOT_VALIDATION_0.1.symbols',
    ],
    depends: validation_library,
  )
endif

validation_abi_layout_test = executable(
  'plainroot-validation-abi-layout-test',
  'tests/abi_layout_test.c',
  include_directories: public_inc,
  install: false,
)
test('validation-abi-layout', validation_abi_layout_test)

installed_consumer_test = find_program('tests/installed_consumer_test.sh')
installed_consumer_dependencies = [validation_library, validation_cli]

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
  installed_consumer_dependencies += [lua_validation_module]

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

test(
  'validation-installed-consumers',
  installed_consumer_test,
  args: [
    meson.current_build_dir(),
    meson.current_source_dir(),
    get_option('prefix'),
    get_option('libdir'),
  ],
  depends: installed_consumer_dependencies,
  suite: 'packaging',
  timeout: 120,
)

fuzzing_feature = get_option('fuzzing')
if fuzzing_feature.enabled()
  cpp = meson.get_compiler('cpp')
  fuzz_compile_args = [
    '-fsanitize=fuzzer-no-link,address,undefined',
    '-fno-omit-frame-pointer',
  ]
  fuzz_link_args = ['-fsanitize=fuzzer,address,undefined']
  if (
    cpp.get_id() != 'clang' or
    not cpp.has_multi_arguments(fuzz_compile_args) or
    not cpp.has_multi_arguments(fuzz_link_args)
  )
    error(
      'Fuzz targets require Clang with libFuzzer, AddressSanitizer, and UBSan support'
    )
  endif

  fuzz_validation_library = static_library(
    'plainroot-validation-fuzz-core',
    'src/report.cpp',
    'src/numeric.cpp',
    'src/text.cpp',
    'src/regex.cpp',
    include_directories: public_inc,
    dependencies: [re2_dep],
    cpp_args: fuzz_compile_args,
    install: false,
  )

  foreach fuzz_case : [
    ['numeric', 'fuzz/numeric_fuzz.cpp'],
    ['text-utf8-regex', 'fuzz/text_utf8_regex_fuzz.cpp'],
  ]
    fuzz_target = executable(
      'plainroot-validation-' + fuzz_case[0] + '-fuzz',
      fuzz_case[1],
      include_directories: public_inc,
      link_with: fuzz_validation_library,
      dependencies: [re2_dep],
      cpp_args: fuzz_compile_args,
      link_args: fuzz_link_args,
      install: false,
    )
    test(
      'validation-fuzz-smoke-' + fuzz_case[0],
      fuzz_target,
      args: ['-runs=1000', '-max_len=4096', '-timeout=2'],
      env: ['ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1'],
      timeout: 30,
    )
  endforeach
endif
