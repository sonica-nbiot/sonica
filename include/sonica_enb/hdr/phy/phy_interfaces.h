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

#ifndef SRSENB_NB_PHY_INTERFACES_H_
#define SRSENB_NB_PHY_INTERFACES_H_


#include <inttypes.h>
#include <srslte/asn1/rrc_asn1_nbiot.h>
#include <srslte/common/interfaces_common.h>
#include <srslte/phy/channel/channel.h>
#include <vector>

namespace sonica_enb {

struct phy_cell_cfg_nb_t {
  srslte_nbiot_cell_t cell;
  uint32_t            rf_port;
  uint32_t            cell_id;
  double              dl_freq_hz;
  double              ul_freq_hz;
};

struct phy_args_t {
  std::string            type;
  srslte::phy_log_args_t log;

  float       max_prach_offset_us = 10;
  int         pusch_max_its       = 10;
  bool        pusch_8bit_decoder  = false;
  float       tx_amplitude        = 1.0f;
  int         nof_phy_threads     = 1;
  std::string equalizer_mode      = "mmse";
  float       estimator_fil_w     = 1.0f;
  bool        pusch_meas_epre     = true;
  bool        pusch_meas_evm      = false;
  bool        pusch_meas_ta       = true;
  bool        emulate_nprach      = false;
};

struct phy_cfg_t {
  // Individual cell/sector configuration list
  phy_cell_cfg_nb_t phy_cell_cfg;

  // Common configuration for all cells
  asn1::rrc::nprach_cfg_sib_nb_r13_s    nprach_cnfg;
  asn1::rrc::npusch_cfg_common_nb_r13_s npusch_cnfg;
};

} // namespace sonica_enb

#endif // SRSENB_NB_PHY_INTERFACES_H_