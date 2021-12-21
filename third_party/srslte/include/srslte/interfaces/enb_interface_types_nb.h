/*
 * Copyright 2013-2020 Software Radio Systems Limited
 * Copyright 2021      Metro Group @ UCLA
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

#ifndef SRSLTE_ENB_INTERFACE_TYPES_NB_H
#define SRSLTE_ENB_INTERFACE_TYPES_NB_H

#include "srslte/asn1/rrc_asn1_nbiot.h"

namespace sonica_enb {

struct cell_cfg_nb_t {
	asn1::rrc::mib_nb_s::operation_mode_info_r13_c_::types mode;
  uint32_t                 rf_port;
  uint32_t                 cell_id;
  uint16_t                 tac;
  uint32_t                 pci;
  uint32_t                 dl_earfcn;
  asn1::rrc::ch_raster_offset_nb_r13_e dl_offset;
  double                   dl_freq_hz;
  uint32_t                 ul_earfcn;
  double                   ul_freq_hz;
  asn1::rrc::carrier_freq_nb_r13_s::carrier_freq_offset_r13_e_ ul_carrier_offset;
};

}

#endif // SRSLTE_ENB_INTERFACE_TYPES_NB_H