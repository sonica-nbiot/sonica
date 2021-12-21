srsepc_mme_srcs = [
  'mme.cc',
  'mme_gtpc.cc',
  'nas.cc',
  's1ap.cc',
  's1ap_ctx_mngmt_proc.cc',
  's1ap_mngmt_proc.cc',
  's1ap_nas_transport.cc',
  's1ap_paging.cc',
  's11u_ep.cc'
]

srsepc_mme = static_library('srsepc_mme', srsepc_mme_srcs,
  include_directories : [srslte_inc, srsepc_ciot_inc]
)
