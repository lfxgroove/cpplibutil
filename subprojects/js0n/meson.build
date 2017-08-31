project('js0n', 'c')

sources = ['src/js0n.c']

js0n_inc = include_directories('src/')
js0n_lib = static_library('js0n', sources, include_directories: js0n_inc)
js0n_dep = declare_dependency(sources: sources, include_directories: js0n_inc, version: '1.0')