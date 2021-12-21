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

#ifndef SONICA_ENB_DL_NBIOT_H
#define SONICA_ENB_DL_NBIOT_H

#include "sonica/config.h"

#include "srslte/phy/ch_estimation/chest_dl_nbiot.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/dft/ofdm.h"
#include "srslte/phy/phch/npbch.h"
#include "srslte/phy/phch/npdcch.h"
#include "srslte/phy/phch/npdsch.h"
#include "srslte/phy/sync/npss.h"
#include "srslte/phy/sync/nsss.h"

#include "srslte/phy/utils/vector.h"

typedef struct SONICA_API {
  srslte_nbiot_cell_t cell;

  cf_t* sf_symbols[SRSLTE_MAX_PORTS];

  srslte_ofdm_t ifft[SRSLTE_MAX_PORTS];

  srslte_npbch_t           npbch;
  srslte_npdcch_t          npdcch;
  srslte_npdsch_t          npdsch;
  srslte_npss_synch_t      npss_sync;
  srslte_nsss_synch_t      nsss_sync;
  srslte_chest_dl_nbiot_t  ch_est;

  cf_t npss_signal[SRSLTE_NPSS_TOT_LEN];
  cf_t nsss_signal[SRSLTE_NSSS_LEN * SRSLTE_NSSS_NUM_SEQ];

  uint8_t bch_payload[SRSLTE_MIB_NB_LEN];
  srslte_mib_nb_t          mib_nb;
} sonica_enb_dl_nbiot_t;

/* This function shall be called just after the initial synchronization */
SONICA_API int sonica_enb_dl_nbiot_init(sonica_enb_dl_nbiot_t* q, cf_t* out_buffer[SRSLTE_MAX_PORTS]);

SONICA_API void sonica_enb_dl_nbiot_free(sonica_enb_dl_nbiot_t* q);

SONICA_API int sonica_enb_dl_nbiot_set_cell(sonica_enb_dl_nbiot_t* q, srslte_nbiot_cell_t cell);

SONICA_API void sonica_enb_dl_nbiot_put_base(sonica_enb_dl_nbiot_t* q, uint32_t hfn, uint32_t tti);

SONICA_API int sonica_enb_dl_nbiot_put_npdcch_dl(sonica_enb_dl_nbiot_t*    q,
                                                 srslte_ra_nbiot_dl_dci_t* dci_dl,
                                                 uint32_t                  sf_idx);
SONICA_API int sonica_enb_dl_nbiot_put_npdcch_ul(sonica_enb_dl_nbiot_t*    q,
                                                 srslte_ra_nbiot_ul_dci_t* dci_ul,
                                                 uint16_t                  rnti,
                                                 uint32_t                  sf_idx);

SONICA_API int sonica_enb_dl_nbiot_put_npdsch(sonica_enb_dl_nbiot_t* q,
                                              srslte_npdsch_cfg_t*   cfg,
                                              uint8_t*               data,
                                              uint16_t               rnti);

SONICA_API void sonica_enb_dl_nbiot_gen_signal(sonica_enb_dl_nbiot_t* q);

#endif // SONICA_ENB_DL_NBIOT_H
