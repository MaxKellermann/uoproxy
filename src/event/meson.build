event = static_library(
  'event',
  'Loop.cxx',
  'ShutdownListener.cxx',
  'TimerWheel.cxx',
  'CoarseTimerEvent.cxx',
  'DeferEvent.cxx',
  'SocketEvent.cxx',
  'SignalEvent.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
  ],
)

event_dep = declare_dependency(
  link_with: event,
  dependencies: [
    system_dep,
    util_dep,
  ],
)
