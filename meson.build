project('cpplibutil', 'cpp', default_options: ['cpp_std=c++11'])
thread_dep = dependency('threads')
boost_dep = dependency('boost', modules:  ['variant', 'uuid'])

subproject('js0n')
js0n_dep = dependency('js0n', fallback: ['js0n', 'js0n_dep'])

sources = ['util.cpp', 'config.cpp', 'json.cpp', 'json_unstructured.cpp', 'logging.cpp']
test_sources = ['tests/test_main.cpp', 'tests/json.cpp', 'tests/logging.cpp']

util_inc = include_directories('./include/')
util_dep = declare_dependency(dependencies: [js0n_dep, boost_dep], include_directories: [util_inc], sources: sources) # link_with: [util_lib],

install_headers('include/util.h', 'include/config.h', 'include/json.h', 'include/json_unstructured.h', 'include/logging.h')

if not meson.is_subproject()
  tests = executable('tests', test_sources, dependencies: [util_dep, js0n_dep, boost_dep, thread_dep])
  test('util tests', tests)
endif
