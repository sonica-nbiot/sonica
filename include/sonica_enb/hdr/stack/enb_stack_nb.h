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

/******************************************************************************
 * File:        enb_stack_nb.h
 * Description: L2/L3 NB-IoT eNB stack class.
 *****************************************************************************/

#ifndef SRSLTE_ENB_NB_STACK_NB_H
#define SRSLTE_ENB_NB_STACK_NB_H

#include "mac/mac.h"
#include "rrc/rrc.h"
#include "upper/pdcp.h"
#include "upper/rlc.h"
#include "upper/s1ap.h"

#include "enb_stack_base.h"
#include "sonica_enb/hdr/enb_nb.h"
#include "srslte/common/logger.h"
#include "srslte/common/mac_pcap.h"
#include "srslte/common/network_utils.h"
#include "srslte/common/s1ap_pcap.h"
#include "srslte/common/threads.h"
#include "srslte/interfaces/enb_interfaces_nb.h"

namespace sonica_enb {

class enb_stack_nb final : public enb_stack_base,
                           public stack_interface_mac_nb,
                           public stack_interface_s1ap_nb,
                           public stack_interface_phy_nb,
                           public srslte::thread
{
public:
  enb_stack_nb(srslte::logger* logger_);
  ~enb_stack_nb();

  // eNB stack base interface
  int         init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_, phy_interface_stack_nb* phy_);
  int         init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_);
  void        stop() final;
  std::string get_type() final;

  /* PHY-MAC interface */
  void rach_detected(uint32_t tti, uint32_t preamble_idx, uint32_t time_adv) final
  {
     mac.rach_detected(tti, preamble_idx, time_adv);
  }
  int get_dl_sched(uint32_t hfn, uint32_t tti, dl_sched_list_t& dl_sched_res) final { return mac.get_dl_sched(hfn, tti, dl_sched_res); }
  int get_ul_sched(uint32_t hfn, uint32_t tti_tx_ul, ul_sched_list_t& ul_sched_res) final { return mac.get_ul_sched(hfn, tti_tx_ul, ul_sched_res); }
  int crc_info(uint32_t tti, uint16_t rnti, uint32_t nof_bytes, bool crc_res) final {return mac.crc_info(tti, rnti, nof_bytes, crc_res);}

  // Radio-Link status
  // void rl_failure(uint16_t rnti) final { mac.rl_failure(rnti); }
  // void rl_ok(uint16_t rnti) final { mac.rl_ok(rnti); }
  void tti_clock() override;

  /* STACK-S1AP interface*/
  void add_mme_socket(int fd) override;
  void remove_mme_socket(int fd) override;
//  void add_gtpu_s1u_socket_handler(int fd) override;
//  void add_gtpu_m1u_socket_handler(int fd) override;

  /* Stack-MAC interface */
  srslte::timer_handler::unique_timer    get_unique_timer() final;
  srslte::task_multiqueue::queue_handler make_task_queue() final;
  void                                   defer_callback(uint32_t duration_ms, std::function<void()> func) final;
  void                                   enqueue_background_task(std::function<void(uint32_t)> task) final;
  void                                   notify_background_task_result(srslte::move_task_t task) final;
  void                                   defer_task(srslte::move_task_t task) final;

private:
  static const int STACK_MAIN_THREAD_PRIO = -1; // Use default high-priority below UHD
  // thread loop
  void run_thread() override;
  void stop_impl();
  void tti_clock_impl();

  void handle_mme_rx_packet(srslte::unique_byte_buffer_t pdu,
                            const sockaddr_in&           from,
                            const sctp_sndrcvinfo&       sri,
                            int                          flags);

  // args
  stack_args_t args    = {};
  rrc_cfg_t    rrc_cfg = {};

  // components that layers depend on (need to be destroyed after layers)
  srslte::timer_handler                           timers;
  std::unique_ptr<srslte::rx_multisocket_handler> rx_sockets;

  sonica_enb::mac       mac;
  srslte::mac_pcap     mac_pcap;
  sonica_enb::rlc       rlc;
  sonica_enb::pdcp      pdcp;
  sonica_enb::rrc       rrc;
  // sonica_enb::gtpu      gtpu;
  sonica_enb::s1ap      s1ap;
  srslte::s1ap_pcap s1ap_pcap;

  srslte::logger*           logger = nullptr;
  srslte::byte_buffer_pool* pool   = nullptr;

  // Radio and PHY log are in enb.cc
  srslte::log_ref mac_log{"MAC"};
  srslte::log_ref rlc_log{"RLC"};
  srslte::log_ref pdcp_log{"PDCP"};
  srslte::log_ref rrc_log{"RRC"};
  srslte::log_ref s1ap_log{"S1AP"};
  srslte::log_ref gtpu_log{"GTPU"};
  srslte::log_ref stack_log{"STCK"};

  // RAT-specific interfaces
  phy_interface_stack_nb* phy = nullptr;

  // state
  bool                    started = false;
  srslte::task_multiqueue pending_tasks;
  int enb_queue_id = -1, sync_queue_id = -1, mme_queue_id = -1, gtpu_queue_id = -1, mac_queue_id = -1,
      stack_queue_id = -1;
  std::vector<srslte::move_task_t>     deferred_stack_tasks; ///< enqueues stack tasks from within. Avoids locking
  // srslte::block_queue<stack_metrics_t> pending_stack_metrics;
};

} // sonica_enb

#endif // SRSLTE_ENB_NB_STACK_NB_H