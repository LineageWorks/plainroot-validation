project(
  'plainroot-validation-installed-consumer', ['c', 'cpp'],
  version: '1.0.0',
  meson_version: '>= 1.3.0',
  default_options: ['c_std=c11', 'cpp_std=c++23', 'warning_level=3'],
)

validation_dep = dependency(
  'plainroot-validation',
  method: 'pkg-config',
  required: true,
)

c_consumer = executable(
  'validation-installed-c-consumer',
  'consumer.c',
  dependencies: validation_dep,
)
test('installed-c-consumer', c_consumer)

cpp_consumer = executable(
  'validation-installed-cpp-consumer',
  'consumer.cpp',
  dependencies: validation_dep,
)
test('installed-cpp-consumer', cpp_consumer)
