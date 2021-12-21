sonica_nbframework = static_library('sonica_nbframework', 'nbframework.cc',
  include_directories : [sonica_inc, srslte_inc, oai_inc],
)

nb_basic = executable('nb_basic', 'nb_basic.cc',
  include_directories : [sonica_inc, srslte_inc, oai_inc],
  link_with : [
    sonica,
    sonica_nbframework,
    srslte_common,
    srslte_phy,
    srslte_radio,
    srslte_rrc_asn1,
  ],
  dependencies: [pthread]
)

nb_ul_decode = executable('nb_ul_decode', 'nb_ul_decode.cc',
  include_directories : [sonica_inc, srslte_inc, oai_inc],
  link_with : [
    sonica,
    sonica_nbframework,
    srslte_common,
    srslte_phy,
    srslte_radio,
    srslte_rrc_asn1,
  ],
  dependencies: [pthread]
)

