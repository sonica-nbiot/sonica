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

#ifndef SONICA_NPRACH_H
#define SONICA_NPRACH_H

#include "sonica/config.h"
#include "srslte/config.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/dft/dft.h"
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SONICA_NPRACH_SAMP_SIZE 512
#define SONICA_NPRACH_SYM_GROUP_SIZE 5
#define SONICA_NPRACH_SUBCARRIERS 12
#define SONICA_NPRACH_SUBC_HALF (SONICA_NPRACH_SUBCARRIERS / 2)

typedef struct SONICA_API {
  srslte_dft_plan_t fft;
  srslte_dft_plan_t ifft;

  cf_t* fft_in;
  cf_t* fft_out;
  cf_t* freq_shift;

  uint32_t cp_len;     // two formats
  uint32_t base_subc;  // base subcarrier

  uint32_t buf_samp;   // Buffered sample from previous sf
  uint32_t cur_symg;   // Current symbol group ID
  uint32_t nxt_sym;    // Next expected symbol in symbol group (0 means reading CP)
  uint32_t total_samp; // Total number of sample read

  cf_t det_buf[20][12];
} sonica_nprach_t;

SONICA_API int sonica_nprach_init(sonica_nprach_t *q);
SONICA_API int sonica_nprach_set_cell(sonica_nprach_t *q);
SONICA_API int sonica_nprach_free(sonica_nprach_t *q);

SONICA_API int sonica_nprach_detect_reset(sonica_nprach_t *q);
SONICA_API int sonica_nprach_detect(sonica_nprach_t* q,
                                    cf_t*            signal,
                                    uint32_t         sig_len,
                                    uint32_t*        need_samples,
                                    uint32_t*        indices,
                                    uint32_t*        ind_len);

#endif // SONICA_NPRACH_H