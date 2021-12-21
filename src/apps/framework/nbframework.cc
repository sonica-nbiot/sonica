/*
 * Copyright 2013-2020 Software Radio Systems Limited
 * Copyright 2021      Metro Group @ UCLA
 *
 * This file is part of Sonica.
 *
 * Sonica is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Sonica is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include "nbframework.h"

#include "srslte/asn1/rrc_asn1_nbiot.h"

int get_sib2_params(const uint8_t* sib1_payload, const uint32_t len, srslte_nbiot_si_params_t* sib2_params)
{
  memset(sib2_params, 0, sizeof(srslte_nbiot_si_params_t));

  // unpack SIB
  asn1::rrc::bcch_dl_sch_msg_nb_s dlsch_msg;
  asn1::cbit_ref                  dlsch_bref(sib1_payload, len);
  asn1::SRSASN_CODE               err = dlsch_msg.unpack(dlsch_bref);
  if (err != asn1::SRSASN_SUCCESS) {
    fprintf(stderr, "Error unpacking DL-SCH message\n");
    return SRSLTE_ERROR;
  }

  // set SIB2-NB parameters
  sib2_params->n              = 1;
  auto sched_info             = dlsch_msg.msg.c1().sib_type1_r13().sched_info_list_r13.begin();
  sib2_params->si_periodicity = sched_info->si_periodicity_r13.to_number();
  if (dlsch_msg.msg.c1().sib_type1_r13().si_radio_frame_offset_r13_present) {
    sib2_params->si_radio_frame_offset = dlsch_msg.msg.c1().sib_type1_r13().si_radio_frame_offset_r13;
  }
  sib2_params->si_repetition_pattern = sched_info->si_repeat_pattern_r13.to_number();
  sib2_params->si_tb                 = sched_info->si_tb_r13.to_number();
  sib2_params->si_window_length      = dlsch_msg.msg.c1().sib_type1_r13().si_win_len_r13.to_number();

  return SRSLTE_SUCCESS;
}

NBFramework::NBFramework()
{
  nof_rf_ports = 1;

  rxport_map[0] = DL_FREQ;
  rxport_map[1] = DL_FREQ;
  txport_map[0] = DL_FREQ;
  txport_map[1] = DL_FREQ;

  have_sib1 = false;
  have_sib2 = false;

  first_xmit = true;
  xmit_enable = false;
}

NBFramework::~NBFramework()
{
  srslte_ue_sync_nbiot_free(&ue_sync);
}

static char empty_str[] = "";

void NBFramework::args_default()
{
  args.use_prf_file             = false;

  args.rf_args                  = empty_str;
  args.rf_device                = nullptr;
  args.rf_freq_dl               = -1.0;
  args.rf_ul_freq_delta         = 0.0;
  args.prf_file                 = nullptr;
  args.rf_gain_rx               = 70.0;
  args.rf_gain_tx               = 70.0;

  args.rnti                     = SRSLTE_SIRNTI;
  args.n_id_ncell               = SRSLTE_CELL_ID_UNKNOWN;
  args.is_r14                   = true;
  args.skip_sib2                = false;
  args.disable_cfo              = false;
}

void NBFramework::parse_args(int argc, char *argv[])
{
  int opt;
  args_default();

  while ((opt = getopt(argc, argv, "amgRBlHCdDsvrfqwzxcFSGU")) != -1) {
    switch (opt) {
      case 'a':
        args.rf_args = argv[optind];
        break;
      case 'm':
        args.rf_device = argv[optind];
        break;
      case 'g':
        args.rf_gain_rx = strtof(argv[optind], NULL);
        break;
      case 'G':
        args.rf_gain_tx = strtof(argv[optind], NULL);
        break;
      case 'C':
        args.disable_cfo = true;
        break;
      case 'f':
        args.rf_freq_dl = strtod(argv[optind], NULL);
        break;
      case 'U':
        if (argv[optind][0] != 'm') {
          args.rf_ul_freq_delta = strtof(argv[optind], NULL);
        } else {
          args.rf_ul_freq_delta = -strtof(&argv[optind][1], NULL);
        }
      case 'r':
        args.rnti = strtol(argv[optind], NULL, 16);
        break;
      case 'l':
        args.n_id_ncell = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'R':
        args.is_r14 = !args.is_r14;
        break;
      case 's':
        args.skip_sib2 = !args.skip_sib2;
        break;
      case 'v':
        srslte_verbose++;
        break;
      case 'S':
        args.nof_adv_samples = (int32_t)strtol(argv[optind], NULL, 10);
        break;
      default:
        // usage(args, argv[0]);
        exit(-1);
    }
  }
  if (args.rf_freq_dl < 0) {
    // usage(args, argv[0]);
    exit(-1);
  }
}

int NBFramework::rf_recv(cf_t* x[SRSLTE_MAX_PORTS], uint32_t nsamples, srslte_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ---- \n", nsamples);
  return srslte_rf_recv_with_time_multi(&rf, (void **)x, nsamples, true, &t->full_secs, &t->frac_secs);
}

int NBFramework::init()
{
  int ret = SRSLTE_ERROR;

  cell.base.nof_prb = SRSLTE_NBIOT_DEFAULT_NUM_PRB_BASECELL;
  cell.nbiot_prb    = SRSLTE_NBIOT_DEFAULT_PRB_OFFSET;
  cell.n_id_ncell   = args.n_id_ncell;
  cell.is_r14       = args.is_r14;

  ret = init_rf();
  if (ret != SRSLTE_SUCCESS) {
    fprintf(stderr, "Failed to initiate RF\n");
    return ret;
  }

  if (srslte_ue_sync_nbiot_init_multi(&ue_sync, SRSLTE_NBIOT_MAX_PRB,
    [](void* h, cf_t* x[SRSLTE_MAX_PORTS], uint32_t nsamples, srslte_timestamp_t* t) {
      NBFramework *f = static_cast<NBFramework *>(h);
      return f->rf_recv(x, nsamples, t);
    }, nof_rf_ports, (void*)this)) {
                                      // srslte_rf_recv_wrapper, nof_rf_ports, (void*)this)) {
    fprintf(stderr, "Error initiating ue_sync\n");
    exit(-1);
  }
  // reduce AGC period to every 10th frame
  srslte_ue_sync_nbiot_set_agc_period(&ue_sync, 10);

  buff_ptrs[0] = srslte_vec_cf_malloc(SRSLTE_SF_LEN_PRB_NBIOT * 10);
  buff_ptrs[1] = srslte_vec_cf_malloc(SRSLTE_SF_LEN_PRB_NBIOT * 10);

  if (buff_ptrs[0] == nullptr || buff_ptrs[1] == nullptr) {
    perror("malloc");
    return SRSLTE_ERROR;
  }

  if (srslte_ue_mib_nbiot_init(&ue_mib, buff_ptrs, SRSLTE_NBIOT_MAX_PRB)) {
    fprintf(stderr, "Error initaiting UE MIB decoder\n");
    return SRSLTE_ERROR;
  }
  if (srslte_ue_mib_nbiot_set_cell(&ue_mib, cell) != SRSLTE_SUCCESS) {
    fprintf(stderr, "Error setting cell configuration in UE MIB decoder\n");
    return SRSLTE_ERROR;
  }

  sf_n_re      = 2 * SRSLTE_CP_NORM_NSYMB * cell.base.nof_prb * SRSLTE_NRE;
  sf_n_samples = 2 * SRSLTE_SLOT_LEN(srslte_symbol_sz(cell.base.nof_prb));

  for (int i = 0; i < 2; i++) {
    tx_re_symbols[i] = srslte_vec_cf_malloc(sf_n_re);
    if (!tx_re_symbols[i]) {
      perror("malloc");
      return SRSLTE_ERROR;
    }

    tx_sf_symbols[i] = srslte_vec_cf_malloc(sf_n_samples);
    if (!tx_sf_symbols[i]) {
      perror("malloc");
      return SRSLTE_ERROR;
    }

    if (srslte_ofdm_tx_init(&ifft[i], SRSLTE_CP_NORM, tx_re_symbols[i], tx_sf_symbols[i], cell.base.nof_prb)) {
      fprintf(stderr, "Error creating iFFT object\n");
      exit(-1);
    }
    srslte_ofdm_set_normalize(&ifft[i], true);
    srslte_ofdm_set_freq_shift(&ifft[i], -SRSLTE_NBIOT_FREQ_SHIFT_FACTOR);
  }

  zero_buffer = srslte_vec_cf_malloc(sf_n_samples);
  if (!zero_buffer) {
    perror("malloc");
    exit(-1);
  }
  srslte_vec_cf_zero(zero_buffer, sf_n_samples);

  time_adv_sec = (float)args.nof_adv_samples / srslte_sampling_freq_hz(cell.base.nof_prb);

  return SRSLTE_SUCCESS;
}

int NBFramework::init_rf()
{
  if (!args.use_prf_file) {
    printf("Opening RF device...\n");
    if (srslte_rf_open_devname(&rf, args.rf_device, args.rf_args, nof_rf_ports)) {
      fprintf(stderr, "Error opening rf\n");
      return SRSLTE_ERROR;
    }
    /* Set receiver gain */
    if (args.rf_gain_rx > 0) {
      printf("Set RX gain: %.1f dB\n", srslte_rf_set_rx_gain(&rf, args.rf_gain_rx));
      printf("Set TX gain: %.1f dB\n", srslte_rf_set_tx_gain(&rf, args.rf_gain_tx));
    } else {
      return SRSLTE_ERROR;
    }

    float dl_freq = args.rf_freq_dl;
    float ul_freq = args.rf_freq_dl + args.rf_ul_freq_delta;

    // set transceiver frequency
    printf("Set RX1 freq: %.6fMHz\n",
           srslte_rf_set_rx_freq(&rf, 0, rxport_map[0] == UL_FREQ ? ul_freq : dl_freq) / 1e6);
    if (nof_rf_ports > 1) {
      printf("Set RX2 freq: %.6fMHz\n",
             srslte_rf_set_rx_freq(&rf, 1, rxport_map[1] == UL_FREQ ? ul_freq : dl_freq) / 1e6);
    }
    printf("Set TX1 freq: %.6fMHz\n",
           srslte_rf_set_tx_freq(&rf, 0, txport_map[0] == UL_FREQ ? ul_freq : dl_freq) / 1e6);
    if (nof_rf_ports > 1) {
      printf("Set TX2 freq: %.6fMHz\n",
             srslte_rf_set_tx_freq(&rf, 1, txport_map[1] == UL_FREQ ? ul_freq : dl_freq) / 1e6);
    }

    // set sampling frequency
    int srate = srslte_sampling_freq_hz(cell.base.nof_prb);
    cur_tx_srate = srate;
    if (srate != -1) {
      printf("Setting sampling rate %.2f MHz\n", (float)srate / 1000000);
      float srate_rf = srslte_rf_set_rx_srate(&rf, (double)srate);
      if (srate_rf != srate) {
        fprintf(stderr, "Could not set sampling rate\n");
        return SRSLTE_ERROR;
      }
      srslte_rf_set_tx_srate(&rf, (double)srate);
    } else {
      fprintf(stderr, "Invalid number of PRB %d\n", cell.base.nof_prb);
      return SRSLTE_ERROR;
    }
  } else {
    return SRSLTE_ERROR;
  }

  return SRSLTE_SUCCESS;
}

void NBFramework::run()
{
  int ret;
  int n;
  float cfo = 0;
  uint8_t  bch_payload[SRSLTE_MIB_NB_LEN] = {};
  int      sfn_offset;
  uint32_t nframes = 0;
  float    rsrp = 0.0, rsrq = 0.0, noise = 0.0;

  srslte_rf_start_rx_stream(&rf, false);

  ue_sync.correct_cfo = !args.disable_cfo;
  // Set initial CFO for ue_sync

  srslte_ue_sync_nbiot_set_cfo(&ue_sync, cfo);
  srslte_npbch_decode_reset(&ue_mib.npbch);
  INFO("\nEntering main loop...\n\n");

  while (!go_exit) {
    ret = srslte_ue_sync_nbiot_zerocopy_multi(&ue_sync, buff_ptrs);
    // printf("RK\n");
    if (ret < 0) {
      fprintf(stderr, "Error calling srslte_nbiot_ue_sync_zerocopy_multi()\n");
      break;
    }

    xmit_enable = false;
    srslte_vec_cf_zero(tx_re_symbols[0], sf_n_re);
    srslte_vec_cf_zero(tx_re_symbols[1], sf_n_re);
    srslte_vec_cf_zero(tx_sf_symbols[0], sf_n_samples);
    srslte_vec_cf_zero(tx_sf_symbols[1], sf_n_samples);

    if (ret == 1) {
      // printf("\nSync'ed\n");
      switch (state) {
        case DECODE_MIB:
          if (get_sfidx() == 0) {
            n = srslte_ue_mib_nbiot_decode(&ue_mib, buff_ptrs[0], bch_payload, &cell.nof_ports, &sfn_offset);
            if (n < 0) {
              fprintf(stderr, "Error decoding UE MIB\n");
              exit(-1);
            } else if (n == SRSLTE_UE_MIB_NBIOT_FOUND) {
              printf("MIB received (CFO: %+6.2f kHz)\n", srslte_ue_sync_nbiot_get_cfo(&ue_sync) / 1000);
              srslte_mib_nb_t mib;
              srslte_npbch_mib_unpack(bch_payload, &mib);

              // update SFN and set deployment mode
              system_frame_number = (mib.sfn + sfn_offset) % 1024;
              cell.mode = mib.mode;

              // set number of ports of base cell to that of NB-IoT cell (FIXME: read eutra-NumCRS-Ports-r13)
              cell.base.nof_ports = cell.nof_ports;

              if (cell.mode == SRSLTE_NBIOT_MODE_INBAND_SAME_PCI) {
                cell.base.id = cell.n_id_ncell;
              }

              if (SRSLTE_VERBOSE_ISINFO()) {
                srslte_mib_nb_printf(stdout, cell, &mib);
              }

              // Initialize DL
              if (srslte_nbiot_ue_dl_init(&ue_dl, buff_ptrs, SRSLTE_NBIOT_MAX_PRB, SRSLTE_NBIOT_NUM_RX_ANTENNAS)) {
                fprintf(stderr, "Error initiating UE downlink processing module\n");
                exit(-1);
              }

              if (srslte_nbiot_ue_dl_set_cell(&ue_dl, cell)) {
                fprintf(stderr, "Configuring cell in UE DL\n");
                exit(-1);
              }

              // Configure downlink receiver with the MIB params and the RNTI we use
              srslte_nbiot_ue_dl_set_mib(&ue_dl, mib);
              srslte_nbiot_ue_dl_set_rnti(&ue_dl, args.rnti);

              if (nof_rf_ports > 1 && rxport_map[1] == UL_FREQ) {
                if (sonica_enb_ul_nbiot_init(&enb_ul, buff_ptrs[1])) {
                  fprintf(stderr, "Error initiating ENB uplink processing module\n");
                  exit(-1);
                }

                if (sonica_enb_ul_nbiot_set_cell(&enb_ul, cell)) {
                  fprintf(stderr, "Configuring cell in ENB UL\n");
                  exit(-1);
                }
              }

              // Pretty-print MIB
              srslte_bit_pack_vector(bch_payload, rx_tb, SRSLTE_MIB_NB_CRC_LEN);
#ifdef ENABLE_GUI
              if (bcch_bch_to_pretty_string(
                      rx_tb, SRSLTE_MIB_NB_CRC_LEN, mib_buffer_decode, sizeof(mib_buffer_decode))) {
                fprintf(stderr, "Error decoding MIB\n");
              }
#endif

#if HAVE_PCAP
              // write to PCAP
              pcap_pack_and_write(pcap_file,
                                  rx_tb,
                                  SRSLTE_MIB_NB_CRC_LEN,
                                  0,
                                  true,
                                  system_frame_number * SRSLTE_NOF_SF_X_FRAME,
                                  0,
                                  DIRECTION_DOWNLINK,
                                  NO_RNTI);
#endif
              // activate SIB1 decoding
              srslte_nbiot_ue_dl_decode_sib1(&ue_dl, system_frame_number);
              state = DECODE_SIB;
            }
          }
          break;
        case DECODE_SIB:
          if (!have_sib1) {
            int dec_ret = srslte_nbiot_ue_dl_decode_npdsch(&ue_dl,
                                                           buff_ptrs[0],
                                                           rx_tb,
                                                           system_frame_number,
                                                           get_sfidx(),
                                                           SRSLTE_SIRNTI);
            if (dec_ret == SRSLTE_SUCCESS) {
              printf("SIB1 received\n");
              srslte_sys_info_block_type_1_nb_t sib = {};
              srslte_npdsch_sib1_unpack(rx_tb, &sib);
              hyper_frame_number = sib.hyper_sfn;

              have_sib1 = true;

              // Decode SIB1 and extract SIB2 scheduling params
              get_sib2_params(rx_tb, ue_dl.npdsch_cfg.grant.mcs[0].tbs / 8, &sib2_params);

              // Activate SIB2 decoding
              srslte_nbiot_ue_dl_decode_sib(
                  &ue_dl, hyper_frame_number, system_frame_number, SRSLTE_NBIOT_SI_TYPE_SIB2, sib2_params);
#if HAVE_PCAP
              pcap_pack_and_write(pcap_file,
                                  rx_tb,
                                  ue_dl.npdsch_cfg.grant.mcs[0].tbs / 8,
                                  0,
                                  true,
                                  system_frame_number * 10 + get_sfidx(),
                                  SRSLTE_SIRNTI,
                                  DIRECTION_DOWNLINK,
                                  SI_RNTI);
#endif
              // if SIB1 was decoded in this subframe, skip processing it further
              break;
            } else if (dec_ret == SRSLTE_ERROR) {
              // reactivate SIB1 grant
              if (srslte_nbiot_ue_dl_has_grant(&ue_dl) == false) {
                srslte_nbiot_ue_dl_decode_sib1(&ue_dl, system_frame_number);
              }
            }
          } else if (!have_sib2 && !srslte_nbiot_ue_dl_is_sib1_sf(
                                       &ue_dl, system_frame_number, srslte_ue_sync_nbiot_get_sfidx(&ue_sync))) {
            // SIB2 is transmitted over multiple subframes, so this needs to be called more than once ..
            int dec_ret = srslte_nbiot_ue_dl_decode_npdsch(&ue_dl,
                                                           buff_ptrs[0],
                                                           rx_tb,
                                                           system_frame_number,
                                                           srslte_ue_sync_nbiot_get_sfidx(&ue_sync),
                                                           SRSLTE_SIRNTI);
            if (dec_ret == SRSLTE_SUCCESS) {
              printf("SIB2 received\n");
              have_sib2 = true;

#if HAVE_PCAP
              pcap_pack_and_write(pcap_file,
                                  rx_tb,
                                  ue_dl.npdsch_cfg.grant.mcs[0].tbs / 8,
                                  0,
                                  true,
                                  system_frame_number * 10 + srslte_ue_sync_nbiot_get_sfidx(&ue_sync),
                                  SRSLTE_SIRNTI,
                                  DIRECTION_DOWNLINK,
                                  SI_RNTI);
#endif
            } else {
              // reactivate SIB2 grant
              if (srslte_nbiot_ue_dl_has_grant(&ue_dl) == false) {
                srslte_nbiot_ue_dl_decode_sib(
                    &ue_dl, hyper_frame_number, system_frame_number, SRSLTE_NBIOT_SI_TYPE_SIB2, sib2_params);
              }
            }
          }

          if (have_sib1 && (have_sib2 || args.skip_sib2)) {
            if (args.rnti == SRSLTE_SIRNTI) {
              srslte_nbiot_ue_dl_decode_sib1(&ue_dl, system_frame_number);
            }
            state = DECODE_NPDSCH;
          }
          break;
        case DECODE_NPDSCH:
          if (args.rnti != SRSLTE_SIRNTI) {
            // printf("S1A\n");
            subframe_handler();
            // printf("S1B\n");
          } else {
            // decode SIB1 over and over again
            n = srslte_nbiot_ue_dl_decode_npdsch(&ue_dl,
                                                 buff_ptrs[0],
                                                 rx_tb,
                                                 system_frame_number,
                                                 get_sfidx(),
                                                 args.rnti);

            // reactivate SIB1 grant
            if (srslte_nbiot_ue_dl_has_grant(&ue_dl) == false) {
              srslte_nbiot_ue_dl_decode_sib1(&ue_dl, system_frame_number);
            }
          }

          rsrq  = SRSLTE_VEC_EMA(srslte_chest_dl_nbiot_get_rsrq(&ue_dl.chest), rsrq, 0.1);
          rsrp  = SRSLTE_VEC_EMA(srslte_chest_dl_nbiot_get_rsrp(&ue_dl.chest), rsrp, 0.05);
          noise = SRSLTE_VEC_EMA(srslte_chest_dl_nbiot_get_noise_estimate(&ue_dl.chest), noise, 0.05);
          nframes++;
          if (isnan(rsrq)) {
            rsrq = 0;
          }
          if (isnan(noise)) {
            noise = 0;
          }
          if (isnan(rsrp)) {
            rsrp = 0;
          }

          // Plot and Printf
          
          if (get_sfidx() == 5) {
            printf(
                "CFO: %+6.2f kHz, RSRP: %4.1f dBm "
                "SNR: %4.1f dB, RSRQ: %4.1f dB, "
                "NPDCCH detected: %d, NPDSCH-BLER: %5.2f%% (%d of total %d), NPDSCH-Rate: %5.2f kbit/s\r",
                srslte_ue_sync_nbiot_get_cfo(&ue_sync) / 1000,
                10 * log10(rsrp),
                10 * log10(rsrp / noise),
                10 * log10(rsrq),
                ue_dl.nof_detected,
                (float)100 * ue_dl.pkt_errors / ue_dl.pkts_total,
                ue_dl.pkt_errors,
                ue_dl.pkts_total,
                (ue_dl.bits_total / ((system_frame_number * 10 + get_sfidx()) / 1000.0)) /
                    1000.0);
          }
          

          break;
      }

      if (get_sfidx() == 9) {
        system_frame_number++;
        if (system_frame_number == 1024) {
          system_frame_number = 0;
          hyper_frame_number++;
          printf("\n");

          // don't reset counter when reading from file to maintain complete stats
          if (!args.use_prf_file) {
            ue_dl.pkt_errors   = 0;
            ue_dl.pkts_total   = 0;
            ue_dl.nof_detected = 0;
            ue_dl.bits_total   = 0;
          }
        }
      }

      if (!first_xmit || xmit_enable) {
        srslte_timestamp_t tstamp;
        srslte_ue_sync_nbiot_get_last_timestamp(&ue_sync, &tstamp);
        srslte_timestamp_add(&tstamp, 0, 4e-3 - time_adv_sec);
        // srslte_rf_send_timed_multi(&rf, sf_symbols, sf_n_samples, tstamp.full_secs, tstamp.frac_secs, true, first_xmit, false);
        // printf("TA\n");
        handle_tx(tx_sf_symbols, sf_n_samples, tstamp);
        // printf("TB\n");
        first_xmit = false;
      }
    } else if (ret == 0) {
      state = DECODE_MIB;

      printf("Finding PSS... Peak: %8.1f, FrameCnt: %d, State: %d\r",
             srslte_sync_nbiot_get_peak_value(&ue_sync.sfind),
             ue_sync.frame_total_cnt,
             ue_sync.state);

      if (!first_xmit || xmit_enable) {
        tx_end();
        first_xmit = true;
      }
    }

    
  }

  srslte_rf_close(&rf);
  printf("\nBye\n");
}

void NBFramework::subframe_handler()
{
  int n;

  if (srslte_nbiot_ue_dl_has_grant(&ue_dl)) {
    // attempt to decode NPDSCH
    n = srslte_nbiot_ue_dl_decode_npdsch(&ue_dl,
                                         buff_ptrs[0],
                                         rx_tb,
                                         system_frame_number,
                                         get_sfidx(),
                                         args.rnti);
    if (n == SRSLTE_SUCCESS) {
      INFO("NPDSCH decoded ok.\n");
      // log_data(rx_tb, last_len);
    }
  } else {
    // decode NPDCCH
    srslte_dci_msg_t dci_msg;
    n = srslte_nbiot_ue_dl_decode_npdcch(&ue_dl,
                                         buff_ptrs[0],
                                         system_frame_number,
                                         get_sfidx(),
                                         args.rnti,
                                         &dci_msg);
    if (n == SRSLTE_NBIOT_UE_DL_FOUND_DCI && dci_msg.format != SRSLTE_DCI_FORMATN0) {
      INFO("DCI found for rnti=%d\n", args.rnti);
      // convert DCI to grant
      srslte_ra_nbiot_dl_dci_t   dci_unpacked;
      srslte_ra_nbiot_dl_grant_t grant;
      if (srslte_nbiot_dci_msg_to_dl_grant(&dci_msg,
                                           args.rnti,
                                           &dci_unpacked,
                                           &grant,
                                           system_frame_number,
                                           srslte_ue_sync_nbiot_get_sfidx(&ue_sync),
                                           64 /* TODO: remove */,
                                           cell.mode)) {
        fprintf(stderr, "Error unpacking DCI\n");
        return;
      }
      // activate grant
      srslte_nbiot_ue_dl_set_grant(&ue_dl, &grant);
      // log_dci(&dci_unpacked, &grant);
      // last_len = grant.mcs[0].tbs / 8;
    }
  }
}

void NBFramework::handle_tx(cf_t **sf_symbols, uint32_t nof_samples, srslte_timestamp_t tx_time)
{
  void *tx_buf[SRSLTE_MAX_PORTS] = { NULL, NULL, NULL, NULL };
  uint32_t sample_offset = 0;

  if (!first_xmit) {
    srslte_timestamp_t ts_overlap = end_of_burst_time;
    srslte_timestamp_sub(&ts_overlap, tx_time.full_secs, tx_time.frac_secs);
    int32_t past_nsamples = (int32_t)round(cur_tx_srate * srslte_timestamp_real(&ts_overlap));

    if (past_nsamples > 0) {
      // If the overlap length is greater than the current transmission length, it means the whole transmission is in
      // the past and it shall be ignored
      // printf("-%d\n", past_nsamples);
      if ((int32_t)nof_samples < past_nsamples) {
        return;
      } else {
        // Trim the first past_nsamples
        sample_offset = (uint32_t)past_nsamples;     // Sets an offset for moving first samples offset
        tx_time       = end_of_burst_time;           // Keeps same transmission time
        nof_samples   = nof_samples - past_nsamples; // Subtracts the number of trimmed samples
      }
    } else if (past_nsamples < 0) {
      uint32_t gap_nsamples = abs(past_nsamples);
      // printf("+%u\n", gap_nsamples);
      while (gap_nsamples > 0) {
        // Transmission cannot exceed SRSLTE_SF_LEN_MAX (zeros buffer size limitation)
        uint32_t nzeros = SRSLTE_MIN(gap_nsamples, sf_n_samples);

        // Zeros transmission
        srslte_rf_send_timed2(
            &rf, zero_buffer, nzeros, end_of_burst_time.full_secs, end_of_burst_time.frac_secs, false, false);
        // if (ret < SRSLTE_SUCCESS) {
        //   return false;
        // }

        // Substract gap samples
        gap_nsamples -= nzeros;

        // Increase timestamp
        srslte_timestamp_add(&end_of_burst_time, 0, (double)nzeros / cur_tx_srate);
      }
    }
  }

  // Save possible end of burst time
  srslte_timestamp_copy(&end_of_burst_time, &tx_time);
  srslte_timestamp_add(&end_of_burst_time, 0, (double)nof_samples / cur_tx_srate);

  //tx_buf[0] = &sf_symbols[0][sample_offset];
  //tx_buf[1] = &sf_symbols[1][sample_offset];
  tx_buf[0] = sf_symbols[0];
  tx_buf[1] = sf_symbols[1];

  srslte_rf_send_timed_multi(&rf, tx_buf, nof_samples, tx_time.full_secs, tx_time.frac_secs, true, first_xmit, false);
}

void NBFramework::tx_end()
{
  srslte_rf_send_timed2(&rf, zero_buffer, 0, end_of_burst_time.full_secs, end_of_burst_time.frac_secs, false, true);
}

bool NBFramework::go_exit = false;

void NBFramework::register_signal()
{
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
  signal(SIGINT, [](int signo) {
    printf("SIGINT received. Exiting...\n");
    if (signo == SIGINT) {
      NBFramework::go_exit = true;
    }
    // exit(0);
  });
}
