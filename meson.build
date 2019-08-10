
project('mmm', 'c',
  license: 'MIT',
  version: '0.0.0',
  meson_version: '>=0.50.0',
)


conf = configuration_data()
pkgconfig = import('pkgconfig')

cc        = meson.get_compiler('c')
prefix    = get_option('prefix')

version = meson.project_version()
array_version = version.split('.')
major_version = array_version[0].to_int()
minor_version = array_version[1].to_int()
micro_version = array_version[2].to_int()
interface_age = 1

binary_age = 100 * minor_version + micro_version

lt_current = binary_age - interface_age

api_version = '@0@.@1@'.format(major_version, minor_version)
lib_version = '@0@:@1@:@2@'.format(lt_current, interface_age, lt_current)
so_version  = '@0@.@1@.@2@'.format(0, lt_current, interface_age)
lib_name    = meson.project_name() + '-' + api_version

stability_version_number = (major_version != 0 ? minor_version : micro_version)
stable = (stability_version_number % 2 == 0)

stability_version_number = (major_version != 0 ? minor_version : micro_version)
stable = (stability_version_number % 2 == 0)

conf.set10('MMM_UNSTABLE', not stable, Description:
  'Define to 1 if this is an unstable version of MMM.')

conf.set       ('MMM_MAJOR_VERSION',    '@0@'.format(major_version))
conf.set       ('MMM_MINOR_VERSION',    '@0@'.format(minor_version))
conf.set       ('MMM_MICRO_VERSION',    '@0@'.format(micro_version))
conf.set_quoted('MMM_INTERFACE_AGE',    '@0@'.format(interface_age))
conf.set_quoted('MMM_BINARY_AGE',       '@0@'.format(binary_age))
conf.set_quoted('MMM_VERSION',          '@0@'.format(version))
conf.set_quoted('MMM_REAL_VERSION',     '@0@'.format(version))
conf.set_quoted('MMM_API_VERSION',      '@0@'.format(api_version))
conf.set_quoted('MMM_RELEASE',          '@0@'.format(api_version))
conf.set_quoted('MMM_LIBRARY_VERSION',  '@0@'.format(lib_version))
conf.set_quoted('MMM_CURRENT_MINUS_AGE','@0@'.format(0))
conf.set_quoted('MMM_LIBRARY',          '@0@'.format(lib_name))

alsa   = dependency('alsa')
thread = dependency('threads')

common_c_flags = []

extra_warnings_list = [
  '-Wdeclaration-after-statement',
  '-Winit-self',
  '-Wmissing-declarations',
  '-Wmissing-prototypes',
  '-Wold-style-definition',
  '-Wpointer-arith',
]
common_c_flags += cc.get_supported_arguments(extra_warnings_list)


sdl1     = dependency('sdl',         version: '>=1.2.0',
  required: get_option('sdl1')
)


rootInclude = include_directories('.')

subdir('lib')
subdir('bin')
subdir('examples')

# pkg-config file
pkgconfig.generate(filebase: 'mmm',
  name: 'mmm',
  description: 'Memory mapped machine',
  version: meson.project_version(),
  libraries: [ mmm_lib ],
  libraries_private: [
    '-lm',
  ],
  subdirs: [
    lib_name,
  ],
)
