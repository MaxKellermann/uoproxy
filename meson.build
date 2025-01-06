project('uoproxy', 'cpp', version: '0.5',
  meson_version: '>= 1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',

    # This is only here to build subprojects as static libraries
    'default_library=static',

    'fmt:cpp_std=c++2a',
    'fmt:warning_level=0',
  ],
  license: 'GPL-2.0-only',
)

compiler = meson.get_compiler('cpp')

common_global_cppflags = [
  '-D_GNU_SOURCE',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

test_common_flags = [
  '-ffast-math',
  '-ftree-vectorize',

  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wmissing-declarations',
  '-Wshadow',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
]

test_global_cxxflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  '-Wno-missing-field-initializers',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

test_ldflags = [
]

if get_option('buildtype') != 'debug'
  test_global_cxxflags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]
  test_ldflags += [
    '-Wl,--gc-sections',
    '-Wl,--icf=all',
  ]
endif

add_global_arguments(common_global_cppflags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')
add_project_link_arguments(compiler.get_supported_link_arguments(test_ldflags), language: 'cpp')

conf = configuration_data()

libsystemd = dependency('libsystemd', required: get_option('systemd'))
conf.set('HAVE_LIBSYSTEMD', libsystemd.found())

configure_file(output: 'config.h', configuration: conf)

version_conf = configuration_data()
version_conf.set_quoted('VERSION', meson.project_version())
configure_file(output: 'version.h', configuration: version_conf)

inc = include_directories(
  'src',

  # for generated headers
  '.',
)

subdir('src/lib/fmt')
subdir('src/util')
subdir('src/io')
subdir('src/system')
subdir('src/memory')
subdir('src/net')
subdir('src/event')
subdir('src/event/net')

executable(
  'uoproxy',
  'src/Main.cxx',
  'src/Config.cxx',
  'src/Instance.cxx',
  'src/Listener.cxx',
  'src/Log.cxx',
  'src/SocketConnect.cxx',
  'src/SocketBuffer.cxx',
  'src/ProxySocks.cxx',
  'src/Encryption.cxx',
  'src/Server.cxx', 'src/Client.cxx',
  'src/uo/Length.cxx',
  'src/Compression.cxx',
  'src/uo/MakePacket.cxx',
  'src/uo/Version.cxx',
  'src/uo/WalkState.cxx',
  'src/CVersion.cxx', 'src/Bridge.cxx',
  'src/Connection.cxx', 'src/CClient.cxx', 'src/CServer.cxx', 'src/CNet.cxx',
  'src/LinkedServer.cxx',
  'src/StatefulClient.cxx',
  'src/World.cxx', 'src/CWorld.cxx', 'src/Walk.cxx',
  'src/SHandler.cxx', 'src/CHandler.cxx',
  'src/Attach.cxx', 'src/Reconnect.cxx',
  'src/Dump.cxx',
  'src/SUtil.cxx',
  'src/Command.cxx',
  'src/UdpKnock.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    libsystemd,
    event_dep,
    event_net_dep,
    memory_dep,
    io_dep,
    net_dep,
    util_dep,
  ],
  install: true,
)

if libsystemd.found()
  subdir('systemd')
endif

subdir('test')
