subdir('phy')
subdir('stack')

sonica_enb_cfg_parser = static_library('sonica_enb_cfg_parser',
  ['parser.cc', 'enb_cfg_parser.cc'],
  include_directories : [sonica_inc, srslte_inc]
)

sonica_enb = executable('sonica_enb', ['main.cc', 'enb_nb.cc'],
  include_directories : [sonica_inc, srslte_inc, cli11_inc],
  link_with : [
    sonica_enb_phy,
    sonica_enb_mac,
    sonica_enb_rrc,
    sonica_enb_upper,
    sonica_enb_stack,
    srslte_common,
    srslte_mac,
    srslte_phy,
    srslte_upper,
    srslte_radio,
    sonica_enb_cfg_parser,
    sonica,
    srslte_rrc_asn1,
    srslte_s1ap_asn1
  ],
  dependencies: [
    pthread,
    sec_dep,
    libconfigpp,
    libsctp
  ]
)
