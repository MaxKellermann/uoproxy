project('uoproxy', 'c', version: '0.5',
  meson_version: '>= 0.47',
  default_options: [
    'c_std=gnu99',
    'warning_level=3',
  ],
  license: 'GPLv2+',
)

compiler = meson.get_compiler('c')

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

test_cflags = test_common_flags + [
  '-Wmissing-prototypes',
  '-Wshadow',
  '-Wpointer-arith',
  '-Wstrict-prototypes',
  '-Wcast-qual',
  '-Wwrite-strings',
  '-pedantic',
]

test_ldflags = [
]

if get_option('buildtype') != 'debug'
  test_cflags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]
  test_ldflags += [
    '-Wl,--gc-sections',
  ]
endif

add_global_arguments(common_cflags + compiler.get_supported_arguments(test_cflags), language: 'c')
add_global_link_arguments(compiler.get_supported_link_arguments(test_ldflags), language: 'cpp')

libevent = dependency('libevent')

inc = include_directories(
  'src',
)

executable(
  'uoproxy',
  'src/uoproxy.c',
  'src/config.c',
  'src/instance.c',
  'src/daemon.c', 'src/log.c',
  'src/socket_connect.c',
  'src/fifo_buffer.c', 'src/flush.c', 'src/socket_buffer.c',
  'src/buffered_io.c', 'src/socket_util.c',
  'src/proxy_socks.c',
  'src/netutil.c',
  'src/encryption.c',
  'src/server.c', 'src/client.c',
  'src/packets.c', 'src/compression.c',
  'src/pversion.c',
  'src/cversion.c', 'src/bridge.c',
  'src/connection.c', 'src/cclient.c', 'src/cserver.c', 'src/cnet.c', 'src/world.c', 'src/cworld.c', 'src/walk.c',
  'src/handler.c', 'src/shandler.c', 'src/chandler.c',
  'src/attach.c', 'src/reconnect.c',
  'src/dump.c',
  'src/sutil.c',
  'src/command.c',
  include_directories: inc,
  dependencies: [
    libevent,
  ],
  install: true,
)
