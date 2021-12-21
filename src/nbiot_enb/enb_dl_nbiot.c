/*
 * Copyright 2013-2020 Software Radio Systems Limited
 * Copyright 2021      Metro Group @UCLA
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

#include "sonica/nbiot_enb/enb_dl_nbiot.h"

#include "srslte/srslte.h"
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <string.h>

#define CURRENT_SFLEN_RE SRSLTE_SF_LEN_RE(SRSLTE_NBIOT_MAX_PRB, SRSLTE_CP_NORM)

int sonica_enb_dl_nbiot_init(sonica_enb_dl_nbiot_t* q, cf_t* out_buffer[SRSLTE_MAX_PORTS])
{
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  uint32_t sf_n_samples = 2 * SRSLTE_SLOT_LEN(srslte_symbol_sz(SRSLTE_NBIOT_MAX_PRB));

  if (q != NULL) {
    ret = SRSLTE_ERROR;

    bzero(q, sizeof(sonica_enb_dl_nbiot_t));

    for (int i = 0; i < SRSLTE_MAX_PORTS; i++) {
      q->sf_symbols[i] = srslte_vec_cf_malloc(SRSLTE_SF_LEN_RE(SRSLTE_NBIOT_MAX_PRB, SRSLTE_CP_NORM));
      if (!q->sf_symbols[i]) {
        perror("malloc");
        goto clean_exit;
      }
    }

    srslte_ofdm_cfg_t ofdm_cfg = {};
    ofdm_cfg.nof_prb           = SRSLTE_NBIOT_MAX_PRB;
    ofdm_cfg.cp                = SRSLTE_CP_NORM;
    ofdm_cfg.normalize         = true;
    ofdm_cfg.freq_shift_f      = -SRSLTE_NBIOT_FREQ_SHIFT_FACTOR;
    for (int i = 0; i < SRSLTE_MAX_PORTS; i++) {
      ofdm_cfg.in_buffer  = q->sf_symbols[i];
      ofdm_cfg.out_buffer = out_buffer[i];
      ofdm_cfg.sf_type    = SRSLTE_SF_NORM;
      if (srslte_ofdm_tx_init_cfg(&q->ifft[i], &ofdm_cfg)) {
        ERROR("Error initiating FFT (%d)\n", i);
        goto clean_exit;
      }
    }

    if (srslte_npbch_init(&q->npbch)) {
      ERROR("Error creating NPBCH object\n");
      goto clean_exit;
    }

    if (srslte_npdcch_init(&q->npdcch)) {
      ERROR("Error creating NPDCCH object\n");
      goto clean_exit;
    }

    if (srslte_npdsch_init(&q->npdsch)) {
      ERROR("Error creating NPDSCH object\n");
      goto clean_exit;
    }

    if (srslte_chest_dl_nbiot_init(&q->ch_est, SRSLTE_NBIOT_MAX_PRB)) {
      ERROR("Error initializing equalizer\n");
      goto clean_exit;
    }

    if (srslte_npss_synch_init(&q->npss_sync, sf_n_samples, srslte_symbol_sz(SRSLTE_NBIOT_MAX_PRB))) {
      ERROR("Error initializing NPSS object\n");
      goto clean_exit;
    }

    if (srslte_nsss_synch_init(&q->nsss_sync, sf_n_samples, srslte_symbol_sz(SRSLTE_NBIOT_MAX_PRB))) {
      ERROR("Error initializing NSSS object\n");
      goto clean_exit;
    }

    // NSSS is cell-specific, should be generated after setting cell
    srslte_npss_generate(q->npss_signal);

    ret = SRSLTE_SUCCESS;
  }

clean_exit:
  if (ret == SRSLTE_ERROR) {
    sonica_enb_dl_nbiot_free(q);
  }
  return ret;
}

void sonica_enb_dl_nbiot_free(sonica_enb_dl_nbiot_t* q)
{
  if (q) {
    for (int i = 0; i < SRSLTE_MAX_PORTS; i++) {
      srslte_ofdm_tx_free(&q->ifft[i]);
    }
    srslte_npbch_free(&q->npbch);
    srslte_npdcch_free(&q->npdcch);
    srslte_npdsch_free(&q->npdsch);
    srslte_chest_dl_nbiot_free(&q->ch_est);
    srslte_npss_synch_free(&q->npss_sync);
    srslte_nsss_synch_free(&q->nsss_sync);
    bzero(q, sizeof(sonica_enb_dl_nbiot_t));
  }
}

int sonica_enb_dl_nbiot_set_cell(sonica_enb_dl_nbiot_t* q, srslte_nbiot_cell_t cell)
{
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  if (q != NULL && srslte_nbiot_cell_isvalid(&cell)) {
    if (q->cell.n_id_ncell != cell.n_id_ncell) {
      q->cell = cell;

      if (srslte_npbch_set_cell(&q->npbch, q->cell)) {
        ERROR("Error setting cell in NPBCH object\n");
        return SRSLTE_ERROR;
      }

      if (srslte_npdcch_set_cell(&q->npdcch, q->cell)) {
        ERROR("Error setting cell in NPDCCH object\n");
        return SRSLTE_ERROR;
      }

      if (srslte_npdsch_set_cell(&q->npdsch, q->cell)) {
        ERROR("Error setting cell in NPDSCH object\n");
        return SRSLTE_ERROR;
      }

      if (srslte_chest_dl_nbiot_set_cell(&q->ch_est, q->cell) != SRSLTE_SUCCESS) {
        ERROR("Error setting channel estimator's cell configuration\n");
        return SRSLTE_ERROR;
      }

      srslte_nsss_generate(q->nsss_signal, q->cell.n_id_ncell);

      // TODO: set MIB parameters according to cell config
      q->mib_nb.sched_info_sib1 = 5; // sched_info_tag;
      q->mib_nb.sys_info_tag    = 14;
      q->mib_nb.ac_barring      = false;
      q->mib_nb.mode            = q->cell.mode;
    }

    ret = SRSLTE_SUCCESS;
  } else {
    ERROR("Invalid cell properties: Id=%d, Ports=%d\n", cell.n_id_ncell, cell.nof_ports);
  }

  return ret;
}

static void clear_sf(sonica_enb_dl_nbiot_t* q)
{
  for (int i = 0; i < q->cell.nof_ports; i++) {
    srslte_vec_cf_zero(q->sf_symbols[i], CURRENT_SFLEN_RE);
  }
}

static void put_npss(sonica_enb_dl_nbiot_t* q)
{
  for (int i = 0; i < q->cell.nof_ports; i++) {
    srslte_npss_put_subframe(&q->npss_sync,
                             q->npss_signal,
                             q->sf_symbols[i],
                             q->cell.base.nof_prb,
                             q->cell.nbiot_prb);
  }
}

static void put_nsss(sonica_enb_dl_nbiot_t* q, uint32_t sfn)
{
  for (int i = 0; i < q->cell.nof_ports; i++) {
    srslte_nsss_put_subframe(&q->nsss_sync,
                             q->nsss_signal,
                             q->sf_symbols[i],
                             sfn,
                             q->cell.base.nof_prb,
                             q->cell.nbiot_prb);
  }
}

static void put_refs(sonica_enb_dl_nbiot_t* q, uint32_t sf_idx)
{
  for (int i = 0; i < q->cell.nof_ports; i++) {
    srslte_refsignal_nrs_put_sf(q->cell, i, q->ch_est.nrs_signal.pilots[i][sf_idx], q->sf_symbols[i]);
  }
}

void sonica_enb_dl_nbiot_put_base(sonica_enb_dl_nbiot_t* q, uint32_t hfn, uint32_t tti)
{
  uint32_t sf_idx = tti % 10;
  uint32_t sfn = tti / 10;

  clear_sf(q);

  // Transmit NPBCH in subframe 0
  if (sf_idx == 0) {
    if ((sfn % SRSLTE_NPBCH_NUM_FRAMES) == 0) {
      srslte_npbch_mib_pack(hfn, sfn, q->mib_nb, q->bch_payload);
    }
    srslte_npbch_put_subframe(&q->npbch, q->bch_payload, q->sf_symbols, sfn);
    // if (SRSLTE_VERBOSE_ISDEBUG()) {
    //   printf("MIB payload: ");
    //   srslte_vec_fprint_hex(stdout, bch_payload, SRSLTE_MIB_NB_LEN);
    // }
  }

  if (sf_idx == 5) {
    // NPSS at subframe 5
    put_npss(q);
  } else if ((sfn % 2 == 0) && sf_idx == 9) {
    // NSSS in every even numbered frame at subframe 9
    put_nsss(q, sfn);
  } else {
    // NRS in all other subframes (using CSR signal intentionally)
    put_refs(q, sf_idx);
  }
}

int sonica_enb_dl_nbiot_put_npdcch_dl(sonica_enb_dl_nbiot_t*    q,
                                      srslte_ra_nbiot_dl_dci_t* dci_dl,
                                      uint32_t                  sf_idx)
{
  srslte_dci_location_t location = { .L = 2, .ncce = 0 };
  srslte_dci_msg_t      dci_msg;
  ZERO_OBJECT(dci_msg);

  if (srslte_dci_msg_pack_npdsch(dci_dl, SRSLTE_DCI_FORMATN1, &dci_msg, false)) {
    ERROR("Error packing DL DCI\n");
    return SRSLTE_ERROR;
  }
  if (srslte_npdcch_encode(&q->npdcch, &dci_msg, location, dci_dl->alloc.rnti, q->sf_symbols, sf_idx)) {
    ERROR("Error encoding DL DCI message\n");
    return SRSLTE_ERROR;
  }

  return SRSLTE_SUCCESS;
}

int sonica_enb_dl_nbiot_put_npdcch_ul(sonica_enb_dl_nbiot_t*    q,
                                      srslte_ra_nbiot_ul_dci_t* dci_ul,
                                      uint16_t                  rnti,
                                      uint32_t                  sf_idx)
{
  // TODO: Support 2 UL locations
  srslte_dci_location_t location = { .L = 2, .ncce = 0 };
  srslte_dci_msg_t      dci_msg;
  ZERO_OBJECT(dci_msg);

  if (srslte_dci_msg_pack_npusch(dci_ul, &dci_msg)) {
    ERROR("Error packing UL DCI\n");
    return SRSLTE_ERROR;
  }
  if (srslte_npdcch_encode(&q->npdcch, &dci_msg, location, rnti, q->sf_symbols, sf_idx)) {
    ERROR("Error encoding DL DCI message\n");
    return SRSLTE_ERROR;
  }

  return SRSLTE_SUCCESS;
}

int sonica_enb_dl_nbiot_put_npdsch(sonica_enb_dl_nbiot_t* q,
                                   srslte_npdsch_cfg_t*   cfg,
                                   uint8_t*               data,
                                   uint16_t               rnti)
{
  return srslte_npdsch_encode_rnti(&q->npdsch, cfg, NULL, data, rnti, q->sf_symbols);
}

void sonica_enb_dl_nbiot_gen_signal(sonica_enb_dl_nbiot_t* q)
{
  //printf("T%d\n", q->cell.nof_ports);
  for (int i = 0; i < q->cell.nof_ports; i++) {
    srslte_ofdm_tx_sf(&q->ifft[i]);
  }
}