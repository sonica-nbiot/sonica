subdir('mme')
subdir('hss')
subdir('spgw')

srsepc = executable('srsepc_ciot', 'main.cc',
  include_directories : [srslte_inc, srsepc_ciot_inc, sonica_inc],
  link_with : [
    srsepc_mme,
    srsepc_hss,
    srsepc_spgw,
    srslte_common,
    srslte_s1ap_asn1,
    srslte_upper,
  ],
  dependencies : [
    pthread,
    boost,
    sec_dep,
    libconfigpp,
    libsctp
  ]
)
