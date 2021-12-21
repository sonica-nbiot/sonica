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

#ifndef SRSENB_NB_PHY_WORKER_H
#define SRSENB_NB_PHY_WORKER_H


#include <list>
#include <mutex>
#include <string.h>

#include "phy_common.h"
#include "srslte/srslte.h"
#include "sonica/sonica.h"

namespace sonica_enb {

class sf_worker : public srslte::thread_pool::worker
{
public:
  explicit sf_worker() = default;
  ~sf_worker();
  void init(phy_common* phy, srslte::log* log_h);
  void reset();

  cf_t* get_buffer_rx(uint32_t antenna_idx);
  cf_t* get_buffer_tx(uint32_t antenna_idx);
  void  set_time(uint32_t hfn, uint32_t tti, uint32_t tx_worker_cnt, srslte_timestamp_t tx_time);

  // int      add_rnti(uint16_t rnti, uint32_t cc_idx, bool is_pcell, bool is_temporal);
  // void     rem_rnti(uint16_t rnti);
  // uint32_t get_nof_rnti();


private:
  void worker_init(phy_common* phy, srslte::log* log_h);
  void work_imp() final;

  void work_ul(stack_interface_phy_nb::ul_sched_list_t ul_grants);
  void work_dl(stack_interface_phy_nb::dl_sched_t& dl_grants,
               stack_interface_phy_nb::ul_sched_t& ul_grants_tx);

  struct dl_grant_record {
    srslte_ra_nbiot_dl_grant_t grant;
    uint16_t                   rnti;
    uint8_t*                   data;
  };

  /* Common objects */
  srslte::log* log_h     = nullptr;
  phy_common*  phy       = nullptr;
  bool         initiated = false;
  bool         running   = false;
  std::mutex   work_mutex;

  uint32_t           hfn = 0;
  uint32_t           tti_rx = 0, tti_tx_dl = 0, tti_tx_ul = 0;
  uint32_t           t_rx = 0, t_tx_dl = 0, t_tx_ul = 0;
  uint32_t           tx_worker_cnt = 0;
  srslte_timestamp_t tx_time       = {};

  cf_t*    signal_buffer_rx[SRSLTE_MAX_PORTS] = {};
  cf_t*    signal_buffer_tx[SRSLTE_MAX_PORTS] = {};

  sonica_enb_dl_nbiot_t enb_dl = {};
  sonica_enb_ul_nbiot_t enb_ul = {};

  srslte_npdsch_cfg_t   sib1_npdsch_cfg = {};
  srslte_npdsch_cfg_t   npdsch_cfg = {};
  bool                  npdsch_active = false;
  uint16_t              npdsch_rnti;
  uint8_t*              npdsch_data;

  uint32_t              npusch_start;
  uint16_t              npusch_rnti;
  uint8_t*              npusch_data;
  srslte_ra_nbiot_ul_grant_t ugrant;
  bool has_ugrant = false;

  std::list<dl_grant_record> dl_pending_grants;
};

} // namespace sonica_enb

#endif // SRSENB_NB_PHY_WORKER_H
