/*
 * Copyright 2013-2020 Software Radio Systems Limited
 * Copyright 2021      Boyan Ding
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

#ifndef SRSLTE_NPUSCH_H
#define SRSLTE_NPUSCH_H

#include "srslte/config.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/dft/dft_precoding.h"
#include "srslte/phy/mimo/precoding.h"
#include "srslte/phy/modem/demod_soft.h"
#include "srslte/phy/modem/mod.h"
#include "srslte/phy/phch/dci.h"
#include "srslte/phy/phch/sch.h"
#include "srslte/phy/scrambling/scrambling.h"


#define SRSLTE_NPUSCH_MAX_NOF_RU 10


/* @brief Narrowband Physical Uplink shared channel (NPUSCH)
 *
 * Reference:    3GPP TS 36.211 version 13.2.0 Release 13 Sec. 10.1.3
 */
typedef struct SRSLTE_API {
  srslte_nbiot_cell_t cell;
  uint32_t            max_re;

  void *g_bits; // Allocated as 16-bit integers, but used as u8 when encoding
  void *q_bits;

  cf_t*   d;
  cf_t*   tx_syms;
  cf_t*   rx_syms;
  cf_t*   ce;

  // tx & rx objects
  srslte_modem_table_t mod_qpsk;
  srslte_sch_t nul_sch;
  srslte_sequence_t tmp_seq;
  srslte_dft_precoding_t dft_precoding;
} srslte_npusch_t;

/*
 * @brief Narrowband Physical uplink shared channel configuration
 *
 * Reference: 3GPP TS 36.211 version 13.2.0 Release 13 Sec. 10.1.3
 */
typedef struct SRSLTE_API {
  srslte_ra_nbiot_ul_grant_t grant;
  srslte_ra_nbits_t          nbits;

  bool                       is_encoded;
  uint32_t                   sf_idx;   // The current idx within the entire NPUSCH
  uint32_t                   rep_idx;  // The current repetion within this NPUSCH
  uint32_t                   num_sf;   // Total number of subframes tx'ed in this NPUSCH
  uint16_t                   rnti;
} srslte_npusch_cfg_t;

SRSLTE_API int srslte_npusch_init_ue(srslte_npusch_t* q);

SRSLTE_API int srslte_npusch_init_enb(srslte_npusch_t* q);

SRSLTE_API void srslte_npusch_free(srslte_npusch_t* q);

SRSLTE_API int srslte_npusch_set_cell(srslte_npusch_t* q, srslte_nbiot_cell_t cell);

SRSLTE_API int srslte_npusch_set_rnti(srslte_npusch_t* q, uint16_t rnti);

SRSLTE_API int srslte_npusch_encode(srslte_npusch_t*        q,
                                    srslte_npusch_cfg_t*    cfg,
                                    srslte_softbuffer_tx_t* softbuffer,
                                    uint8_t*                data,
                                    cf_t*                   sf_symbols);

SRSLTE_API int srslte_npusch_decode(srslte_npusch_t*        q,
                                    srslte_npusch_cfg_t*    cfg,
                                    srslte_softbuffer_rx_t* softbuffer,
                                    cf_t*                   sf_symbols,
                                    cf_t*                   ce,
                                    float                   noise_estimate,
                                    uint8_t*                data);

SRSLTE_API int srslte_npusch_cfg(srslte_npusch_cfg_t*        cfg,
                                 srslte_ra_nbiot_ul_grant_t* grant,
                                 uint16_t                    rnti);

SRSLTE_API int
srslte_npusch_cp(cf_t* input, cf_t* output, srslte_ra_nbiot_ul_grant_t* grant, bool put);

#endif // SRSLTE_NPUSCH_H
