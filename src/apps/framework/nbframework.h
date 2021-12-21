/*
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

#ifndef SONICA_APP_NBFRAMEWORK_H
#define SONICA_APP_NBFRAMEWORK_H

extern "C" {
#include "srslte/phy/rf/rf.h"
}
#include "srslte/phy/rf/rf_utils.h"

extern "C" {
#include "sonica/nbiot_enb/enb_ul_nbiot.h"
#include "srslte/phy/ue/ue_dl_nbiot.h"
#include "srslte/phy/ue/ue_mib_nbiot.h"
#include "srslte/phy/ue/ue_sync_nbiot.h"
}

class NBFramework {
public:
  NBFramework();
  virtual ~NBFramework();

  void parse_args(int argc, char *argv[]);
  void print_help();

  int init();
  void run();

  inline uint32_t get_sfidx() {
    return srslte_ue_sync_nbiot_get_sfidx(&ue_sync);
  }

  static void register_signal();

protected:
  int add_args();

  // virtual void print_help_addon();
  // virtual int init_addon();
  int init_rf();

  virtual void subframe_handler();

  uint32_t nof_rf_ports;

  // arguments
  struct {
    bool use_prf_file;

    char *rf_args;
    char *rf_device;
    double rf_freq_dl;
    double rf_ul_freq_delta;
    float rf_gain_rx;
    float rf_gain_tx;
    int nof_adv_samples;

    char *prf_file;

    bool disable_cfo;
    int n_id_ncell;
    bool is_r14;
    bool skip_sib2;
    uint16_t rnti;
  } args;

  // Controls the mapping of RF port frequencies.
  enum {
    UL_FREQ,
    DL_FREQ,
  } txport_map[2], rxport_map[2];

  srslte_nbiot_cell_t    cell;
  srslte_nbiot_ue_dl_t   ue_dl;
  srslte_nbiot_ue_sync_t ue_sync;
  srslte_ue_mib_nbiot_t  ue_mib;

  sonica_enb_ul_nbiot_t  enb_ul;

  srslte_nbiot_si_params_t sib2_params;

  uint32_t system_frame_number;
  uint32_t hyper_frame_number;

  cf_t* buff_ptrs[SRSLTE_MAX_PORTS];

  uint8_t rx_tb[SRSLTE_MAX_DL_BITS_CAT_NB1];

  // transmitting (forging)
  srslte_ofdm_t          ifft[2];
  cf_t* tx_re_symbols[SRSLTE_MAX_PORTS];
  cf_t* tx_sf_symbols[SRSLTE_MAX_PORTS];
  bool xmit_enable;
  uint32_t sf_n_re;
  uint32_t sf_n_samples;

private:
  enum receiver_state { DECODE_MIB, DECODE_SIB, DECODE_NPDSCH } state;

  bool have_sib1;
  bool have_sib2;

  srslte_rf_t rf;

  bool first_xmit;
  float cur_tx_srate;
  float time_adv_sec;
  srslte_timestamp_t end_of_burst_time;
  cf_t* zero_buffer;

  static bool go_exit;

  void args_default();
  int rf_recv(cf_t* x[SRSLTE_MAX_PORTS], uint32_t nsamples, srslte_timestamp_t* t);
  void handle_tx(cf_t **sf_symbols, uint32_t nof_samples, srslte_timestamp_t tx_time);
  void tx_end();
};

#endif // SONICA_APP_NBFRAMEWORK_H
