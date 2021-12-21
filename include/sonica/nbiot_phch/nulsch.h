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

#ifndef SONICA_NULSCH_H
#define SONICA_NULSCH_H

#include "sonica/config.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/phch/ra_nbiot.h"
#include "srslte/phy/phch/uci.h"

#include <stdint.h>

typedef struct SONICA_API {
  int16_t dummy_w[3][3*(6144+64)];
  int16_t w[3][3*(6144+64)];
  int16_t d[3][3*(6144+64)];
  uint8_t c[3][3*(6144+64)];

  uint8_t *temp_g_bits;
  uint32_t *ul_interleaver;
  srslte_uci_bit_t ack_ri_bits[57600]; // 4*M_sc*Qm_max for RI and ACK
} sonica_nulsch_t __attribute__ ((aligned(32)));;

SONICA_API int sonica_nulsch_init(sonica_nulsch_t *q);
SONICA_API int sonica_nulsch_free(sonica_nulsch_t *q);

SONICA_API int sonica_nulsch_decode(sonica_nulsch_t*            q,
                                    srslte_ra_nbiot_ul_grant_t* grant,
                                    int16_t*                    q_bits,
                                    int16_t*                    g_bits,
                                    uint8_t*                    data);

#endif // SONICA_NULSCH_H