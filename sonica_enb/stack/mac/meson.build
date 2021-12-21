sonica_enb_mac_srcs = files([
  'mac.cc',
  'scheduler.cc',
  'scheduler_carrier.cc',
  'scheduler_grid.cc',
  'scheduler_ue.cc',
  'ue.cc'
])

sonica_enb_mac = static_library('sonica_enb_mac', sonica_enb_mac_srcs,
  include_directories : [sonica_inc, srslte_inc]
)