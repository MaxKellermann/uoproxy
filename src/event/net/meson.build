event_net_sources = [
  'ConnectSocket.cxx',
  'ServerSocket.cxx',
  'SocketWrapper.cxx',
  'BufferedSocket.cxx',
]

event_net = static_library(
  'event_net',
  event_net_sources,
  include_directories: inc,
)

event_net_dep = declare_dependency(
  link_with: event_net,
  dependencies: [
    event_dep,
    net_dep,
    util_dep,
  ],
)
