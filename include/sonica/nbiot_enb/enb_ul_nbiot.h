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

#ifndef SONICA_ENB_UL_NBIOT_H
#define SONICA_ENB_UL_NBIOT_H

#include <stdbool.h>

#include "sonica/config.h"
#include "sonica/nbiot_phch/npusch.h"

#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/common/sequence.h"
#include "srslte/phy/dft/ofdm.h"

#include "srslte/phy/phch/ra_nbiot.h"

#include "srslte/phy/utils/debug.h"
#include "srslte/phy/utils/vector.h"

/*
 * @brief Narrowband ENB uplink object.
 *
 * This module is a frontend to all the uplink data and control
 * channel processing modules.
 */
typedef struct SONICA_API {
  sonica_npusch_t         npusch;
  srslte_ofdm_t           fft;
  // srslte_chest_ul_nbiot_t chest;

  srslte_softbuffer_rx_t softbuffer;
  srslte_nbiot_cell_t    cell;

  int    nof_re;     // Number of RE per subframe
  cf_t*  sf_symbols; // this buffer holds the symbols of the current subframe
  cf_t*  sf_buffer;  // this buffer holds multiple subframes
  cf_t*  ce;
  cf_t*  ce_buffer;

  float noise_estimate;

  srslte_sequence_t ref12_seq;

  // UL configuration for "normal" transmissions
  bool                has_ul_grant;
  sonica_npusch_cfg_t npusch_cfg;
} sonica_enb_ul_nbiot_t;

SONICA_API int sonica_enb_ul_nbiot_init(sonica_enb_ul_nbiot_t* q,
                                        cf_t*                  in_buffer);

SONICA_API void sonica_enb_ul_nbiot_free(sonica_enb_ul_nbiot_t* q);

SONICA_API int sonica_enb_ul_nbiot_set_cell(sonica_enb_ul_nbiot_t* q, srslte_nbiot_cell_t cell);

SONICA_API int
sonica_enb_ul_nbiot_cfg_grant(sonica_enb_ul_nbiot_t* q, srslte_ra_nbiot_ul_grant_t* grant, uint16_t rnti);

SONICA_API int sonica_enb_ul_nbiot_decode_npusch(sonica_enb_ul_nbiot_t* q,
                                                 cf_t*                  input,
                                                 uint32_t               sf_idx,
                                                 uint8_t*               data);

#endif // SONICA_ENB_UL_NBIOT_H
