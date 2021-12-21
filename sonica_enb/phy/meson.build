sonica_enb_phy_srcs = files([
  'nprach_worker.cc',
  'phy.cc',
  'phy_common.cc',
  'sf_worker.cc',
  'txrx.cc'
])

sonica_enb_phy = static_library('sonica_enb_phy', sonica_enb_phy_srcs,
  include_directories : [sonica_inc, srslte_inc]
)