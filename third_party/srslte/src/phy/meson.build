subdir('agc')
subdir('ch_estimation')
subdir('common')
subdir('dft')
subdir('fec')
subdir('io')
subdir('mimo')
subdir('modem')
subdir('phch')
subdir('resampling')
subdir('scrambling')
subdir('sync')
subdir('ue')
subdir('utils')

srslte_phy_srcs = [
  srslte_agc_srcs,
  srslte_chest_srcs,
  srslte_dft_srcs,
  srslte_fec_srcs,
  srslte_io_srcs,
  srslte_mimo_srcs,
  srslte_modem_srcs,
  srslte_phch_srcs,
  srslte_phy_common_srcs,
  srslte_resampling_srcs,
  srslte_scrambling_srcs,
  srslte_sync_srcs,
  srslte_ue_srcs,
  srslte_utils_srcs,
]

srslte_phy = static_library('srslte_phy', srslte_phy_srcs,
  include_directories : srslte_inc,
  dependencies : [fftw3f, pthread, libm]
)

subdir('rf')
