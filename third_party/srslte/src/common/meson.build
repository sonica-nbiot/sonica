sources = files([
  'arch_select.cc',
  'backtrace.c',
  'buffer_pool.cc',
  'crash_handler.c',
  'gen_mch_tables.c',
  'liblte_security.cc',
  'log_filter.cc',
  'logmap.cc',
  'logger_file.cc',
  'mac_pcap.cc',
  'nas_pcap.cc',
  'network_utils.cc',
  'pcap.c',
  'rlc_pcap.cc',
  's1ap_pcap.cc',
  'security.cc',
  'snow_3g.cc',
  'thread_pool.cc',
  'threads.c',
  'tti_sync_cv.cc',
  'time_prof.cc',
  'version.c',
  'zuc.cc'
])

srslte_common = static_library('srslte_common', sources,
  include_directories : srslte_inc
)
