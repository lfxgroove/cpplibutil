project('cpplibutil', 'cpp', default_options: ['cpp_std=c++1z'])
thread_dep = dependency('threads')
boost_dep = dependency('boost', modules:  ['variant', 'uuid'])

subproject('js0n')
js0n_dep = dependency('js0n', fallback: ['js0n', 'js0n_dep'])

sources = ['util.cpp', 'config.cpp', 'json.cpp', 'json_unstructured.cpp', 'logging.cpp']
test_sources = ['tests/test_main.cpp', 'tests/json.cpp', 'tests/logging.cpp']

util_lib = library('util', sources, dependencies: [js0n_dep, boost_dep, thread_dep])
executable('tests', test_sources, link_with: [util_lib], dependencies: [js0n_dep, boost_dep, thread_dep])
