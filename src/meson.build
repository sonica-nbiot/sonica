subdir('common')
subdir('nbiot_enb')
subdir('nbiot_phch')

sonica_srcs = [
  sonica_common_srcs,
  sonica_nbiot_enb_srcs,
  sonica_nbiot_phch_srcs
]

sonica = static_library('sonica', sonica_srcs,
  include_directories : [sonica_inc, srslte_inc, oai_inc],
  link_with : [oai_phy_coding]
)

subdir('apps')
