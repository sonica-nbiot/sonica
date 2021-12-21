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

#ifndef SRSENB_NB_PHY_H
#define SRSENB_NB_PHY_H

#include "phy_common.h"
#include "sf_worker.h"
#include "nprach_worker.h"
// #include "srsenb/hdr/phy/enb_phy_base.h"
#include "srslte/common/log.h"
#include "srslte/common/log_filter.h"
#include "srslte/common/thread_pool.h"
#include "srslte/common/trace.h"
#include "srslte/interfaces/enb_interfaces.h"
// #include "srslte/interfaces/enb_metrics_interface.h"
#include "srslte/interfaces/radio_interfaces.h"
#include "srslte/radio/radio.h"
#include "txrx.h"

namespace sonica_enb {

// TODO: Define a new NB-IoT version of interface between stack and PHY
// sonica_enb::public phy_interface_stack_lte

class phy final : public srslte::phy_interface_radio
{
public:
  phy(srslte::logger* logger_);
  ~phy();

  int  init(const phy_args_t&            args,
            const phy_cfg_t&             cfg,
            srslte::radio_interface_phy* radio_,
            stack_interface_phy_nb*      stack_);
  void stop();

  std::string get_type() { return "nb-iot"; };

  /* MAC->PHY interface */
  // int  add_rnti(uint16_t rnti, uint32_t pcell_index, bool is_temporal) override;
  // void rem_rnti(uint16_t rnti) final;

  /*RRC-PHY interface*/
  // void set_config_dedicated(uint16_t rnti, const phy_rrc_dedicated_list_t& dedicated_list) override;
  // void complete_config_dedicated(uint16_t rnti) override;

  // void get_metrics(phy_metrics_t metrics[ENB_METRICS_MAX_USERS]) override;

  void radio_overflow() override{};
  void radio_failure() override{};

private:
  // phy_rrc_cfg_t phy_rrc_config = {};
  uint32_t      nof_workers    = 0;

  const static int MAX_WORKERS = 4;

  const static int PRACH_WORKER_THREAD_PRIO = 3;
  const static int SF_RECV_THREAD_PRIO      = 1;
  const static int WORKERS_THREAD_PRIO      = 2;

  srslte::radio_interface_phy* radio = nullptr;
  stack_interface_phy_nb*      stack = nullptr;

  srslte::logger*                                   logger = nullptr;
  std::vector<std::unique_ptr<srslte::log_filter> > log_vec;
  srslte::log*                                      log_h = nullptr;

  srslte::thread_pool    workers_pool;
  std::vector<sf_worker> workers;
  phy_common             workers_common;
  nprach_worker          nprach;
  txrx                   tx_rx;

  bool initialized = false;

  // srslte_prach_cfg_t prach_cfg = {};

  // void parse_common_config(const phy_cfg_t& cfg);
};

} // namespace sonica_enb


#endif // SRSENB_NB_PHY_H