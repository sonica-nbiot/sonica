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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sonica/nbiot_phch/nprach.h"

#include "srslte/srslte.h"

#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/dft/dft.h"
#include "srslte/phy/utils/debug.h"
#include "srslte/phy/utils/vector.h"

#define FREQ_SHIFT_MAX_LEN SONICA_NPRACH_SAMP_SIZE * 4 * 6

static void generate_freq_shift(sonica_nprach_t *q)
{
  float ts = 1.0f / 1.92e6;
  float fshift = -1.85e3; // TODO: Really?
  for (uint32_t i = 0; i < FREQ_SHIFT_MAX_LEN; i++) {
    q->freq_shift[i] = cexpf(I * 2 * M_PI * fshift * ts * i);
  }
}

int sonica_nprach_init(sonica_nprach_t *q)
{
  int ret = SRSLTE_ERROR;

  if (q != NULL) {
    bzero(q, sizeof(sonica_nprach_t));

    if (srslte_dft_plan(&q->fft, SONICA_NPRACH_SAMP_SIZE, SRSLTE_DFT_FORWARD, SRSLTE_DFT_COMPLEX)) {
      ERROR("Error creating DFT plan\n");
      return SRSLTE_ERROR;
    }
    srslte_dft_plan_set_mirror(&q->fft, true);
    srslte_dft_plan_set_dc(&q->fft, false);

    q->fft_in  = srslte_vec_cf_malloc(SONICA_NPRACH_SAMP_SIZE);
    q->fft_out = srslte_vec_cf_malloc(SONICA_NPRACH_SAMP_SIZE);
    q->freq_shift = srslte_vec_cf_malloc(FREQ_SHIFT_MAX_LEN);

    generate_freq_shift(q);

    q->cp_len  = SONICA_NPRACH_SAMP_SIZE; // TODO: Format 266.7us only

    q->base_subc = 36;

    ret = SRSLTE_SUCCESS;
  }

  return ret;
}

int sonica_nprach_free(sonica_nprach_t* q)
{
  free(q->fft_in);
  free(q->fft_out);
  free(q->freq_shift);
  srslte_dft_plan_free(&q->fft);

  bzero(q, sizeof(sonica_nprach_t));

  return SRSLTE_SUCCESS;
}

int sonica_nprach_detect_reset(sonica_nprach_t *q)
{
  q->nxt_sym  = 0;
  q->cur_symg = 0;

  return SRSLTE_SUCCESS;
}

static uint32_t get_subcarrier_sg(uint32_t start_sc, uint32_t sg)
{
  uint32_t sc_id;
  // TODO: extend over 4 sgs
  switch (sg % 4) {
  case 0:
    sc_id = start_sc;
    break;
  case 1:
    sc_id = start_sc ^ 1;
    break;
  case 2:
    sc_id = (start_sc < SONICA_NPRACH_SUBC_HALF ?
             start_sc + SONICA_NPRACH_SUBC_HALF :
             start_sc - SONICA_NPRACH_SUBC_HALF) ^ 1;
    break;
  case 3:
    sc_id = start_sc < SONICA_NPRACH_SUBC_HALF ?
            start_sc + SONICA_NPRACH_SUBC_HALF :
            start_sc - SONICA_NPRACH_SUBC_HALF;
    break;
  }

  return sc_id;
}

static void store_detect_buf(cf_t *buf_entry, cf_t *fft_output, uint32_t sg)
{
  for (uint32_t start_sc = 0; start_sc < SONICA_NPRACH_SUBCARRIERS; start_sc++) {
    uint32_t sc = get_subcarrier_sg(start_sc, sg);
    buf_entry[start_sc] = fft_output[sc];
  }
}

int sonica_nprach_detect(sonica_nprach_t *q,
                         cf_t*            signal,
                         uint32_t         sig_len,
                         uint32_t*        need_samples,
                         uint32_t*        indices,
                         uint32_t*        ind_len)
{
  uint32_t buf_offset = q->buf_samp;
  uint32_t in_offset = 0;
  uint32_t rem_input = sig_len;
  int result = 0;
  bool finished = false;

  while (rem_input + buf_offset >= SONICA_NPRACH_SAMP_SIZE) {
    uint32_t read_amount;
    if (q->nxt_sym == 0) {
      // We are still expecting CP, just swallow the samples
      // CP length is no greater than symbol length, so can we expect reading
      // a full CP here
      read_amount = q->cp_len - buf_offset;
      // printf("%d: Read %d samples as CP\n", q->cur_symg, read_amount);
    } else {
      read_amount = SONICA_NPRACH_SAMP_SIZE - buf_offset;
      srslte_vec_cf_copy(&q->fft_in[buf_offset], &signal[in_offset], read_amount);

      srslte_vec_prod_ccc(q->fft_in, q->freq_shift, q->fft_in, SONICA_NPRACH_SAMP_SIZE);
      srslte_dft_run(&q->fft, q->fft_in, q->fft_out);

      uint32_t sym_num = q->cur_symg * SONICA_NPRACH_SYM_GROUP_SIZE + q->nxt_sym - 1;
      uint32_t subc_offset = (SONICA_NPRACH_SAMP_SIZE - 48) / 2 + q->base_subc;
      store_detect_buf(q->det_buf[sym_num], &q->fft_out[subc_offset], q->cur_symg);
    }

    rem_input -= read_amount;
    in_offset += read_amount;
    buf_offset = 0;
    if (q->nxt_sym == SONICA_NPRACH_SYM_GROUP_SIZE) {
      q->nxt_sym = 0;
      q->cur_symg++;
      if (q->cur_symg == 4) {
        // TODO: support multiple groups of 4 SG
        finished = true;
        break;
      }
    } else {
      q->nxt_sym++;
    }
  }

  if (finished) {
    for (int sc = 0; sc < 12; sc++) {
      int count = 0;
      float avg = 0;
      for (int i = 0; i < 20; i++) {
        float absx = cabsf(q->det_buf[i][sc]);
        if (absx > 5) {
          count++;
        }
        avg += absx;
      }
      avg /= 20;
      if (count > 18) {
        printf("DET SUBC %d (avg %f)\n", sc, avg);
        // XXX
        if (indices != NULL) {
          *indices = sc + 36;
        }
        result = 1;
      }

      // printf("Subc %d\n", sc);
      // for (int i = 0; i < 20; i++) {
      //   printf("\t%2.4f", cabsf(q->det_buf[i][sc]));
      //   if (i % 5 == 4) {
      //     printf("\n");
      //   }
      // }
    }
    q->buf_samp = 0;
  } else {
    if (rem_input > 0) {
      srslte_vec_cf_copy(q->fft_in, &signal[in_offset], rem_input);
    }
    q->buf_samp = rem_input;
    // printf("%d.%d: Absorb %d remaining input\n", q->cur_symg, q->nxt_sym - 1, rem_input);
  }

  return result;
}
