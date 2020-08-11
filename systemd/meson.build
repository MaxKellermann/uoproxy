systemd_unit_conf = configuration_data()
systemd_unit_conf.set('prefix', get_option('prefix'))

configure_file(
  input: 'uoproxy.service.in',
  output: 'uoproxy.service',
  configuration: systemd_unit_conf,
  install_dir: 'lib/systemd/system',
)
