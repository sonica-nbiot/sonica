if rf_found
  # srslte_rf_utils = static_library('srslte_rf_utils', 'rf_utils.c',
  #   include_directories : srslte_inc,
  #   link_with : srslte_phy
  # )

  srslte_rf_srcs = files(['rf_imp.c'])
  srslte_rf_deps = []
  srslte_rf_args = []

  if (uhd.found())
    srslte_rf_args += '-DENABLE_UHD'
    srslte_rf_srcs += files(['rf_uhd_imp.c', 'uhd_c_api.cpp'])
    srslte_rf_deps += uhd
  endif

  if (bladerf.found())
    srslte_rf_args += '-DENABLE_BLADERF'
    srslte_rf_srcs += files(['rf_blade_imp.c'])
    srslte_rf_deps += bladerf
  endif

  if (soapysdr.found())
    srslte_rf_args += '-DENABLE_SOAPYSDR'
    srslte_rf_srcs += files(['rf_soapy_imp.c'])
    srslte_rf_deps += soapysdr
  endif

  if (zeromq.found())
    srslte_rf_args += '-DENABLE_ZEROMQ'
    srslte_rf_srcs += files(['rf_zmq_imp.c', 'rf_zmq_imp_rx.c', 'rf_zmq_imp_tx.c'])
    srslte_rf_deps += zeromq
  endif

  srslte_rf = shared_library('srslte_rf', srslte_rf_srcs,
    c_args : srslte_rf_args,
    cpp_args : srslte_rf_args,
    include_directories : srslte_inc,
    dependencies : srslte_rf_deps,
    link_with : [srslte_phy]
  )
endif