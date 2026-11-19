project(
  'iwgui',
  'c',
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>= 1.4.0',
  default_options: [
    'c_std=gnu23',
    'warning_level=3',
    'buildtype=debugoptimized',
  ],
)

cc = meson.get_compiler('c')

extra_cflags = cc.get_supported_arguments([
  '-Wshadow',
  '-Wconversion',
  '-Wdouble-promotion',
  '-Wformat=2',
  '-Wstrict-prototypes',
  '-Wundef',
  '-fno-common',
])

add_project_arguments(['-D_GNU_SOURCE'] + extra_cflags, language: 'c')

inc = include_directories('src')

base_sources = files(
  'src/base/arena.c',
  'src/base/log.c',
  'src/base/map.c',
  'src/base/pool.c',
  'src/base/str.c',
)

base_lib = static_library(
  'iwgui_base',
  base_sources,
  include_directories: inc,
)

test_base = executable(
  'test_base',
  files('tests/test_base.c'),
  link_with: base_lib,
  include_directories: inc,
)

test('base', test_base)
