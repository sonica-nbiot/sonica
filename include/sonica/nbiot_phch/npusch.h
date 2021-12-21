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

#ifndef SONICA_NPUSCH_H
#define SONICA_NPUSCH_H

#include "sonica/config.h"
#include "sonica/nbiot_phch/nulsch.h"

#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/dft/dft_precoding.h"
#include "srslte/phy/mimo/precoding.h"
#include "srslte/phy/modem/demod_soft.h"
#include "srslte/phy/modem/mod.h"
#include "srslte/phy/phch/dci.h"
#include "srslte/phy/phch/sch.h"
#include "srslte/phy/scrambling/scrambling.h"


#define SONICA_NPUSCH_MAX_NOF_RU 10


/* @brief Narrowband Physical Uplink shared channel (NPUSCH)
 *
 * Reference:    3GPP TS 36.211 version 13.2.0 Release 13 Sec. 10.1.3
 */
typedef struct SONICA_API {
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

  sonica_nulsch_t nulsch;
} sonica_npusch_t;

/*
 * @brief Narrowband Physical uplink shared channel configuration
 *
 * Reference: 3GPP TS 36.211 version 13.2.0 Release 13 Sec. 10.1.3
 */
typedef struct SONICA_API {
  srslte_ra_nbiot_ul_grant_t grant;
  srslte_ra_nbits_t          nbits;

  bool                       is_encoded;
  uint32_t                   sf_idx;   // The current idx within the entire NPUSCH
  uint32_t                   rep_idx;  // The current repetion within this NPUSCH
  uint32_t                   num_sf;   // Total number of subframes tx'ed in this NPUSCH
  uint16_t                   rnti;
} sonica_npusch_cfg_t;

SONICA_API int sonica_npusch_init_ue(sonica_npusch_t* q);

SONICA_API int sonica_npusch_init_enb(sonica_npusch_t* q);

SONICA_API void sonica_npusch_free(sonica_npusch_t* q);

SONICA_API int sonica_npusch_set_cell(sonica_npusch_t* q, srslte_nbiot_cell_t cell);

SONICA_API int sonica_npusch_set_rnti(sonica_npusch_t* q, uint16_t rnti);

SONICA_API int sonica_npusch_encode(sonica_npusch_t*        q,
                                    sonica_npusch_cfg_t*    cfg,
                                    srslte_softbuffer_tx_t* softbuffer,
                                    uint8_t*                data,
                                    cf_t*                   sf_symbols);

SONICA_API int sonica_npusch_decode(sonica_npusch_t*        q,
                                    sonica_npusch_cfg_t*    cfg,
                                    srslte_softbuffer_rx_t* softbuffer,
                                    cf_t*                   sf_symbols,
                                    cf_t*                   ce,
                                    float                   noise_estimate,
                                    uint8_t*                data);

SONICA_API int sonica_npusch_cfg(sonica_npusch_cfg_t*        cfg,
                                 srslte_ra_nbiot_ul_grant_t* grant,
                                 uint16_t                    rnti);

SONICA_API int
sonica_npusch_cp(cf_t* input, cf_t* output, srslte_ra_nbiot_ul_grant_t* grant, bool put);

#endif // SONICA_NPUSCH_H
