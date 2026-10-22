project(
  'plainroot-validation-state-integration', 'cpp',
  version: '0.1.0',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=3', 'cpp_std=c++23'],
)

validation_dep = dependency('plainroot-validation', method: 'pkg-config')
state_dep = dependency('plainroot-state', method: 'pkg-config')

age_command = executable(
  'plainroot-validation-state-command',
  'age_command.cpp',
  dependencies: [validation_dep, state_dep],
  install: false,
)

test_script = find_program('test_age_command.sh')
test(
  'validation-before-state-commit',
  test_script,
  args: [age_command.full_path()],
)
