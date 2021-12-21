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

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbframework.h"

class NBULDecode : public NBFramework
{
public:
  NBULDecode();
  ~NBULDecode();

protected:
  virtual void subframe_handler() override;
  void log_data(uint8_t *data, uint32_t len);

private:
  FILE *log_fp;

  uint8_t rx_tb[SRSLTE_MAX_DL_BITS_CAT_NB1]; // Byte buffer for rx'ed transport blocks

  bool ul_record_start;
  uint32_t ul_record_cnt;
  uint32_t ul_record_skip;
};

NBULDecode::NBULDecode()
{
  nof_rf_ports = 2;
  rxport_map[1] = UL_FREQ;

  ul_record_start = false;

  log_fp = fopen("/tmp/nbul_decode.log", "w");
  if (log_fp == nullptr) {
    fprintf(stderr, "Cannot open log file\n");
  }
}

NBULDecode::~NBULDecode()
{
  fclose(log_fp);
}

void NBULDecode::log_data(uint8_t *data, uint32_t len)
{
  fprintf(log_fp, "Data decoding succeed, len = %u\n", len);
  for (uint32_t i = 0; i < len && i < 256; ) {
    for (uint32_t j = 0; j < 16 && i < len; j++, i++) {
      fprintf(log_fp, " %02x", data[i]);
    }
    fprintf(log_fp, "\n");
  }
  fprintf(log_fp, "\n");
}

void NBULDecode::subframe_handler()
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
    if (n == SRSLTE_NBIOT_UE_DL_FOUND_DCI) {
      if (dci_msg.format != SRSLTE_DCI_FORMATN0) {
        INFO("DCI found for rnti=%d\n", args.rnti);
        // convert DCI to grant
        srslte_ra_nbiot_dl_dci_t   dci_unpacked;
        srslte_ra_nbiot_dl_grant_t grant;
        if (srslte_nbiot_dci_msg_to_dl_grant(&dci_msg,
                                             args.rnti,
                                             &dci_unpacked,
                                             &grant,
                                             system_frame_number,
                                             get_sfidx(),
                                             64 /* TODO: remove */,
                                             cell.mode)) {
          fprintf(stderr, "Error unpacking DCI\n");
          return;
        }
        // activate grant
        // srslte_nbiot_ue_dl_set_grant(&ue_dl, &grant);
        // log_dci(&dci_unpacked, &grant);
        // last_len = grant.mcs[0].tbs / 8;
      } else {
        srslte_ra_nbiot_ul_dci_t   udci_unpacked;
        srslte_ra_nbiot_ul_grant_t ugrant;
        if (srslte_nbiot_dci_msg_to_ul_grant(&dci_msg,
                                             &udci_unpacked,
                                             &ugrant,
                                             system_frame_number * 10 + get_sfidx(),
                                             // TODO
                                             SRSLTE_NPUSCH_SC_SPACING_15000)) {
          fprintf(log_fp, "WARNING: CANNOT correctly parse UL DCI");
        } else if (!ul_record_start) {
          fprintf(log_fp, "RECORDED UL DCI @ %d.%d\n",
                  system_frame_number, get_sfidx());
          srslte_ra_npusch_fprint(log_fp, &udci_unpacked);
          srslte_ra_nbiot_ul_grant_fprint(log_fp, &ugrant);
          ul_record_cnt = ugrant.k0 + udci_unpacked.dci_sf_rep_num + ugrant.nof_slots * ugrant.nof_rep / 2 + 5;
          ul_record_skip = ugrant.k0 + udci_unpacked.dci_sf_rep_num + 1;
          ul_record_start = true;
          if (ugrant.nof_sc == 12 && ugrant.nof_rep == 1 && udci_unpacked.dci_sf_rep_num == 0) {
            sonica_enb_ul_nbiot_cfg_grant(&enb_ul, &ugrant, args.rnti);
          }
        }
      }
    }
  }

  if (ul_record_start && ul_record_cnt > 0) {
    ul_record_cnt--;
    if (ul_record_skip == 0) {
      // srslte_cfo_correct(&ue_sync.strack.cfocorr,
      //                    buff_ptrs[1],
      //                    buff_ptrs[1],
      //                    -srslte_sync_nbiot_get_cfo(&ue_sync.strack) / ue_sync.fft_size);
      // fwrite(buff_ptrs[1], sizeof(cf_t), 1920, ul_record_file);
      // ul_record_pos++;
      if (enb_ul.has_ul_grant) {
        n = sonica_enb_ul_nbiot_decode_npusch(&enb_ul,
                                              buff_ptrs[1],
                                              srslte_ue_sync_nbiot_get_sfidx(&ue_sync),
                                              rx_tb);
        if (n == SRSLTE_SUCCESS) {
          log_data(rx_tb, enb_ul.npusch_cfg.grant.mcs.tbs / 8);
        } else {
          fprintf(log_fp, "UL decode returns %d\n", n);
        }
      }
    }
    if (ul_record_skip > 0) {
      ul_record_skip--;
    }
    if (ul_record_cnt == 0) {
      ul_record_start = false;
    }
  }
}

NBULDecode nb;

int main(int argc, char *argv[])
{
  nb.parse_args(argc, argv);

  NBFramework::register_signal();

  nb.init();
  nb.run();

  return 0;
}