/*
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#ifndef SRSLTE_NPRACH_H
#define SRSLTE_NPRACH_H

#include "srslte/config.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/dft/dft.h"
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SRSLTE_NPRACH_SAMP_SIZE 512
#define SRSLTE_NPRACH_SYM_GROUP_SIZE 5
#define SRSLTE_NPRACH_SUBCARRIERS 12
#define SRSLTE_NPRACH_SUBC_HALF (SRSLTE_NPRACH_SUBCARRIERS / 2)

typedef struct SRSLTE_API {
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
} srslte_nprach_t;

SRSLTE_API int srslte_nprach_init(srslte_nprach_t *q);
SRSLTE_API int srslte_nprach_set_cell(srslte_nprach_t *q);
SRSLTE_API int srslte_nprach_free(srslte_nprach_t *q);

SRSLTE_API int srslte_nprach_detect_reset(srslte_nprach_t *q);
SRSLTE_API int srslte_nprach_detect(srslte_nprach_t* q,
                                    cf_t*            signal,
                                    uint32_t         sig_len,
                                    uint32_t*        need_samples,
                                    uint32_t*        indices,
                                    uint32_t*        ind_len);

#endif // SRSLTE_NPRACH_H