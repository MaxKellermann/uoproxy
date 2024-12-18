system = static_library(
  'system',
  'EpollFD.cxx',
  include_directories: inc,
)

system_dep = declare_dependency(
  link_with: system,
  dependencies: [
    io_dep,
    util_dep,
  ],
)
