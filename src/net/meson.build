net = static_library(
  'net',
  'IPv4Address.cxx',
  'IPv6Address.cxx',
  'SocketAddress.cxx',
  'SocketDescriptor.cxx',
  'SocketError.cxx',
  'StaticSocketAddress.cxx',
  include_directories: inc,
)

net_dep = declare_dependency(
  link_with: net,
)
