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

#include "sonica/nbiot_enb/enb_ul_nbiot.h"

#include "srslte/phy/ch_estimation/chest_common.h"
#include "srslte/phy/ch_estimation/refsignal_ul.h"
#include "srslte/phy/ue/ue_dl_nbiot.h"

#include <assert.h>
#include <complex.h>
#include <math.h>
#include <string.h>

// TODO: configure them
#define GROUP_ASSIGNMENT 0
#define N_RU_SEQ 30

static float smooth_filter[3] = {0.3333, 0.3334, 0.3333};

int sonica_enb_ul_nbiot_init(sonica_enb_ul_nbiot_t* q,
                             cf_t*                  in_buffer)
{
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  if (q != NULL) {
    ret = SRSLTE_ERROR;

    bzero(q, sizeof(sonica_enb_ul_nbiot_t));

    q->nof_re = SRSLTE_SF_LEN_RE(SRSLTE_NBIOT_MAX_PRB, SRSLTE_CP_NORM);

    // for transmissions using only single subframe
    q->sf_symbols = srslte_vec_cf_malloc(q->nof_re);
    if (!q->sf_symbols) {
      perror("malloc");
      goto clean_exit;
    }
    srslte_vec_cf_zero(q->sf_symbols, q->nof_re);

    q->ce = srslte_vec_cf_malloc(q->nof_re);
    if (!q->ce) {
      perror("malloc");
      goto clean_exit;
    }
    srslte_vec_cf_zero(q->ce, q->nof_re);

    // allocate memory for symbols and estimates for tx spanning multiple subframes
    // TODO: only buffer softbits rather than raw samples
    q->sf_buffer = srslte_vec_cf_malloc(q->nof_re * SONICA_NPUSCH_MAX_NOF_RU);
    if (!q->sf_buffer) {
      perror("malloc");
      goto clean_exit;
    }
    srslte_vec_cf_zero(q->sf_buffer, q->nof_re * SONICA_NPUSCH_MAX_NOF_RU);

    q->ce_buffer = srslte_vec_cf_malloc(q->nof_re * SONICA_NPUSCH_MAX_NOF_RU);
    if (!q->ce_buffer) {
      perror("malloc");
      goto clean_exit;
    }
    srslte_vec_cf_zero(q->ce_buffer, q->nof_re * SONICA_NPUSCH_MAX_NOF_RU);

    // initialize memory
    srslte_ofdm_cfg_t ofdm_cfg = {};
    ofdm_cfg.nof_prb           = 1;
    ofdm_cfg.in_buffer         = in_buffer;
    ofdm_cfg.out_buffer        = q->sf_symbols;
    ofdm_cfg.cp                = SRSLTE_CP_NORM;
    ofdm_cfg.freq_shift_f      = SRSLTE_NBIOT_FREQ_SHIFT_FACTOR;
    ofdm_cfg.normalize         = false;
    ofdm_cfg.rx_window_offset  = 0.2f;
    if (srslte_ofdm_rx_init_cfg(&q->fft, &ofdm_cfg)) {
      fprintf(stderr, "Error initiating FFT\n");
      goto clean_exit;
    }
    // srslte_ofdm_set_normalize(&q->fft, true);

    if (sonica_npusch_init_enb(&q->npusch)) {
      fprintf(stderr, "Error creating PDSCH object\n");
      goto clean_exit;
    }
    if (srslte_softbuffer_rx_init(&q->softbuffer, 50)) {
      fprintf(stderr, "Error initiating soft buffer\n");
      goto clean_exit;
    }

    ret = SRSLTE_SUCCESS;
  }

clean_exit:
  if (ret == SRSLTE_ERROR) {
    sonica_enb_ul_nbiot_free(q);
  }
  return ret;
}

void sonica_enb_ul_nbiot_free(sonica_enb_ul_nbiot_t* q)
{
  if (q) {
    srslte_ofdm_rx_free(&q->fft);
    sonica_npusch_free(&q->npusch);
    srslte_softbuffer_rx_free(&q->softbuffer);
    if (q->sf_symbols) {
      free(q->sf_symbols);
    }
    if (q->ce) {
      free(q->ce);
    }
    if (q->sf_buffer) {
      free(q->sf_buffer);
    }
    if (q->ce_buffer) {
      free(q->ce_buffer);
    }
    bzero(q, sizeof(sonica_enb_ul_nbiot_t));
  }
}

int sonica_enb_ul_nbiot_set_cell(sonica_enb_ul_nbiot_t* q, srslte_nbiot_cell_t cell)
{
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  if (q != NULL && srslte_nbiot_cell_isvalid(&cell)) {

    if (q->cell.n_id_ncell != cell.n_id_ncell || q->cell.base.nof_prb == 0) {
      q->cell = cell;

      if (sonica_npusch_set_cell(&q->npusch, q->cell)) {
        fprintf(stderr, "Error creating NPUSCH object\n");
        return SRSLTE_ERROR;
      }

      srslte_sequence_LTE_pr(&q->ref12_seq, 640, q->cell.n_id_ncell / N_RU_SEQ);
    }
    ret = SRSLTE_SUCCESS;
  } else {
    fprintf(stderr,
            "Invalid cell properties: n_id_ncell=%d, Ports=%d, base cell's PRBs=%d\n",
            cell.n_id_ncell,
            cell.nof_ports,
            cell.base.nof_prb);
  }
  return ret;
}

int sonica_enb_ul_nbiot_cfg_grant(sonica_enb_ul_nbiot_t* q,
                                  srslte_ra_nbiot_ul_grant_t* grant,
                                  uint16_t rnti)
{
  // configure NPUSCH object
  sonica_npusch_cfg(&q->npusch_cfg, grant, rnti);
  q->has_ul_grant = true;
  q->noise_estimate = 0.0f;

  return SRSLTE_SUCCESS;
}

// TODO: Put these into a separate chest module
/* Uses the difference between the averaged and non-averaged pilot estimates */
static float estimate_noise_pilots(cf_t* noisy, cf_t* noiseless)
{
  cf_t tmp_noise[SRSLTE_NRE] = {};
  float power = 0;

  power = srslte_chest_estimate_noise_pilots(
      noisy,
      noiseless,
      tmp_noise,
      SRSLTE_NRE);

  if (true) {
    // Calibrated for filter length 3
    float w = smooth_filter[0];
    float a = 7.419 * w * w + 0.1117 * w - 0.005387;
    return (power / (a * 0.8));
  } else {
    return power;
  }
}

// The interpolator currently only supports same frequency allocation for each subframe
#define cesymb(i) ce[SRSLTE_RE_IDX(1, i, 0)]
static void interpolate_pilots(cf_t* ce, uint32_t nrefs)
{
  // Instead of a linear interpolation, we just copy the estimates to all symbols in that subframe
  for (int s = 0; s < 2; s++) {
    int src_symb = s * 7 + 3;
    for (int i = 0; i < 7; i++) {
      int dst_symb = i + s * 7;

      // skip the symbol with the estimates
      if (dst_symb != src_symb) {
        memcpy(&ce[dst_symb * SRSLTE_NRE],
               &ce[src_symb * SRSLTE_NRE],
               nrefs * sizeof(cf_t));
      }
    }
  }
}

int sonica_enb_ul_nbiot_decode_fft_estimate(sonica_enb_ul_nbiot_t* q, uint32_t sf_idx)
{
  float noise_avg = 0;
  uint32_t ns = sf_idx * 2;

  srslte_ofdm_rx_sf(&q->fft);

  for (int slot = ns; slot < ns + 2; slot++) {
    uint32_t fgh = 0;
    for (int i = 0; i < 8; i++) {
      fgh += q->ref12_seq.c[slot * 8 + i] << i;
    }
    // TODO: configure group hopping
    uint32_t fss = q->cell.n_id_ncell + GROUP_ASSIGNMENT; // + Hopping Group ID
    uint32_t u = (fgh + fss) % N_RU_SEQ;
    INFO("Slot %d: fgh=%d, fss=%d, u = %d\n", slot, fgh, fss, u);

    float tmp_arg[SRSLTE_NRE];
    cf_t r_uv[SRSLTE_NRE];
    srslte_refsignal_r_uv_arg_1prb(tmp_arg, u);
    for (int i = 0; i < SRSLTE_NRE; i++) {
      r_uv[i] = cexpf(I * tmp_arg[i]);
    }

    cf_t estimate[SRSLTE_NRE];
    cf_t estimate_avg[SRSLTE_NRE];
    srslte_vec_prod_conj_ccc(&q->sf_symbols[((slot - ns) * 7 + 3) * SRSLTE_NRE], r_uv, estimate, SRSLTE_NRE);

    srslte_chest_average_pilots(
        estimate,
        estimate_avg,
        smooth_filter,
        SRSLTE_NRE,
        1,
        3);

    srslte_vec_cf_copy(&q->ce[((slot - ns) * 7 + 3) * SRSLTE_NRE], estimate_avg, SRSLTE_NRE);

    // for (int i = 0; i < SRSLTE_NRE; i++) {
    //   printf("%f => %f + %fj // %f + %fj\n",
    //          tmp_arg[i],
    //          __real__ estimate[i], __imag__ estimate[i],
    //          __real__ estimate_avg[i], __imag__ estimate_avg[i]);
    // }

    float noise_pwr = estimate_noise_pilots(estimate, estimate_avg);
    // float snr = srslte_vec_avg_power_cf(&q->sf_symbols[((slot - ns) * 7 + 3) * SRSLTE_NRE], SRSLTE_NRE) / noise_pwr;
    
    // printf("Noise pwr = %f, snr = %f\n\n", noise_pwr, snr);

    noise_avg += noise_pwr;
  }

  interpolate_pilots(q->ce, SRSLTE_NRE);

  q->noise_estimate += noise_avg;

  return SRSLTE_SUCCESS;
}

int sonica_enb_ul_nbiot_decode_npusch(sonica_enb_ul_nbiot_t* q,
                                      cf_t*                  input,
                                      uint32_t               sf_idx,
                                      uint8_t*               data)
{
  int ret = SRSLTE_ERROR_INVALID_INPUTS;

  if (q->has_ul_grant == false) {
    DEBUG("Skipping NPUSCH processing due to lack of grant.\n");
    return SRSLTE_ERROR_INVALID_INPUTS;
  }

  if ((sonica_enb_ul_nbiot_decode_fft_estimate(q, sf_idx)) < 0) {
    return ret;
  }

  uint32_t cfg_sf = q->npusch_cfg.grant.nof_slots / 2;
  // TODO: support repetition
  if (q->npusch_cfg.num_sf >= cfg_sf) {
    ERROR("We don't handle repetition now\n");
    return SRSLTE_ERROR_INVALID_INPUTS;
  }

  srslte_vec_cf_copy(&q->sf_buffer[q->npusch_cfg.sf_idx * q->nof_re], q->sf_symbols, q->nof_re);
  srslte_vec_cf_copy(&q->ce_buffer[q->npusch_cfg.sf_idx * q->nof_re], q->ce, q->nof_re);

  q->npusch_cfg.num_sf++;
  q->npusch_cfg.sf_idx++;
  if (q->npusch_cfg.num_sf == cfg_sf * q->npusch_cfg.grant.nof_rep) {
    INFO("Trying to decode NPUSCH with %d RU(s), %d SF.\n", q->npusch_cfg.grant.nof_ru, cfg_sf);
    srslte_softbuffer_rx_reset(&q->softbuffer);
    // TODO: make sure noise estimation is correct
    if (sonica_npusch_decode(&q->npusch, &q->npusch_cfg, &q->softbuffer, q->sf_buffer, q->ce_buffer,
                             q->noise_estimate / 2.0f, data) != SRSLTE_SUCCESS) {
      INFO("Error decoding NPUSCH.\n");
      ret = SRSLTE_ERROR;
    } else {
      ret = SRSLTE_SUCCESS;
    }
    q->has_ul_grant = false;
  } else {
    ret = SRSLTE_NBIOT_EXPECT_MORE_SF;
  }

  return ret;
}
