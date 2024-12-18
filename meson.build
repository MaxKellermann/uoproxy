project('uoproxy', 'cpp', version: '0.5',
  meson_version: '>= 1.0',
  default_options: [
    'cpp_std=gnu++17',
    'warning_level=3',
  ],
  license: 'GPL-2.0-only',
)

compiler = meson.get_compiler('cpp')

common_cppflags = [
  '-D_GNU_SOURCE',
]

common_cflags = [
]

test_common_flags = [
  '-Wvla',

  '-fvisibility=hidden',

  '-ffast-math',
  '-ftree-vectorize',
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wmissing-declarations',
  '-Wshadow',
  '-Wpointer-arith',
  '-Wcast-qual',
  '-Wwrite-strings',
  '-Wsign-compare',
  '-Wcomma',
  '-Wextra-semi',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
  '-Wunreachable-code-break',
  '-Wunused',
  '-Wused-but-marked-unused',

  '-Wno-non-virtual-dtor',

  # TODO: remove those after the C++ transition is complete
  '-Wno-pedantic',
]

test_ldflags = [
]

if get_option('buildtype') != 'debug'
  test_cxxflags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]
  test_ldflags += [
    '-Wl,--gc-sections',
    '-Wl,--icf=all',
  ]
endif

add_global_arguments(common_cppflags + compiler.get_supported_arguments(test_cxxflags), language: 'cpp')
add_global_link_arguments(compiler.get_supported_link_arguments(test_ldflags), language: 'cpp')

conf = configuration_data()

libevent = dependency('libevent')

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

executable(
  'uoproxy',
  'src/Main.cxx',
  'src/Config.cxx',
  'src/Instance.cxx',
  'src/Log.cxx',
  'src/SocketConnect.cxx',
  'src/Flush.cxx', 'src/SocketBuffer.cxx',
  'src/BufferedIO.cxx', 'src/SocketUtil.cxx',
  'src/ProxySocks.cxx',
  'src/NetUtil.cxx',
  'src/Encryption.cxx',
  'src/Server.cxx', 'src/Client.cxx',
  'src/PacketLengths.cxx', 'src/Compression.cxx',
  'src/PVersion.cxx',
  'src/CVersion.cxx', 'src/Bridge.cxx',
  'src/Connection.cxx', 'src/CClient.cxx', 'src/CServer.cxx', 'src/CNet.cxx',
  'src/LinkedServer.cxx',
  'src/StatefulClient.cxx',
  'src/World.cxx', 'src/CWorld.cxx', 'src/Walk.cxx',
  'src/Handler.cxx', 'src/SHandler.cxx', 'src/CHandler.cxx',
  'src/Attach.cxx', 'src/Reconnect.cxx',
  'src/Dump.cxx',
  'src/SUtil.cxx',
  'src/Command.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    libsystemd,
  ],
  install: true,
)

if libsystemd.found()
  subdir('systemd')
endif
