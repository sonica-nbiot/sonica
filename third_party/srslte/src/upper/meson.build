srslte_upper_srcs = [
  'gtpu.cc',
  'pdcp.cc',
  'pdcp_entity_base.cc',
  'pdcp_entity_lte.cc',
  'rlc.cc',
  'rlc_am_base.cc',
  'rlc_am_lte.cc',
  'rlc_tm.cc',
  'rlc_um_base.cc',
  'rlc_um_lte.cc'
]

srslte_upper = static_library('srslte_upper', srslte_upper_srcs,
  include_directories : srslte_inc,
  link_with : [srslte_common, srslte_asn1]
)