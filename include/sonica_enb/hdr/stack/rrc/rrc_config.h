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


#ifndef SRSENB_NB_RRC_CONFIG_H
#define SRSENB_NB_RRC_CONFIG_H

#include "srslte/asn1/rrc_asn1_nbiot.h"
#include "srslte/interfaces/enb_interface_types_nb.h"
#include "srslte/phy/common/phy_common.h"

namespace sonica_enb {

struct rrc_cfg_t {
  uint32_t enb_id;
  asn1::rrc::sib_type1_nb_s sib1;
  asn1::rrc::sib_type2_nb_r13_s sib2;
  asn1::rrc::sib_type3_nb_r13_s sib3;

  asn1::rrc::mac_main_cfg_nb_r13_s    mac_cnfg;
  asn1::rrc::npusch_cfg_ded_nb_r13_s  npusch_cfg;
  asn1::rrc::npdcch_cfg_ded_nb_r13_s  npdcch_cfg;
  asn1::rrc::carrier_cfg_ded_nb_r13_s carrier_cfg;

  srslte_nbiot_cell_t cell_common;
  cell_cfg_nb_t       cell_spec;
};

} // namespace sonica_enb

#endif // SRSENB_NB_RRC_CONFIG_H
