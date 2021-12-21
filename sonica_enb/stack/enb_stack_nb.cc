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

#include "sonica_enb/hdr/stack/enb_stack_nb.h"
#include "sonica_enb/hdr/enb_nb.h"
#include "srslte/common/network_utils.h"
#include "srslte/srslte.h"
// #include <srslte/interfaces/enb_metrics_interface.h>

using namespace srslte;

namespace sonica_enb {

enb_stack_nb::enb_stack_nb(srslte::logger* logger_) :
  timers(128), logger(logger_), pdcp(this, "PDCP"), thread("STACK")
{
  enb_queue_id   = pending_tasks.add_queue();
  sync_queue_id  = pending_tasks.add_queue();
  mme_queue_id   = pending_tasks.add_queue();
  gtpu_queue_id  = pending_tasks.add_queue();
  mac_queue_id   = pending_tasks.add_queue();
  stack_queue_id = pending_tasks.add_queue();

  pool = byte_buffer_pool::get_instance();
}

enb_stack_nb::~enb_stack_nb()
{
  stop();
}

std::string enb_stack_nb::get_type()
{
  return "nbiot";
}

int enb_stack_nb::init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_, phy_interface_stack_nb* phy_)
{
  phy = phy_;
  if (init(args_, rrc_cfg_)) {
    return SRSLTE_ERROR;
  }

  return SRSLTE_SUCCESS;
}

int enb_stack_nb::init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_)
{
  args    = args_;
  rrc_cfg = rrc_cfg_;

  // setup logging for each layer
  srslte::logmap::register_log(std::unique_ptr<srslte::log>{new log_filter{"MAC ", logger, true}});
  mac_log->set_level(args.log.mac_level);
  mac_log->set_hex_limit(args.log.mac_hex_limit);

  // Init logs
  rlc_log->set_level(args.log.rlc_level);
  pdcp_log->set_level(args.log.pdcp_level);
  rrc_log->set_level(args.log.rrc_level);
  gtpu_log->set_level(args.log.gtpu_level);
  s1ap_log->set_level(args.log.s1ap_level);
  stack_log->set_level(args.log.stack_level);

  rlc_log->set_hex_limit(args.log.rlc_hex_limit);
  pdcp_log->set_hex_limit(args.log.pdcp_hex_limit);
  rrc_log->set_hex_limit(args.log.rrc_hex_limit);
  gtpu_log->set_hex_limit(args.log.gtpu_hex_limit);
  s1ap_log->set_hex_limit(args.log.s1ap_hex_limit);
  stack_log->set_hex_limit(args.log.stack_hex_limit);

  // Set up pcap and trace
  if (args.mac_pcap.enable) {
    mac_pcap.open(args.mac_pcap.filename.c_str());
    // mac.start_pcap(&mac_pcap);
  }
  if (args.s1ap_pcap.enable) {
    s1ap_pcap.open(args.s1ap_pcap.filename.c_str());
    // s1ap.start_pcap(&s1ap_pcap);
  }

  // Init Rx socket handler
  rx_sockets.reset(new srslte::rx_multisocket_handler("ENBSOCKETS", stack_log));

  // Init all layers
  mac.init(args.mac, /* rrc_cfg.cell_list, phy, */ &rlc, &rrc, this, mac_log);
  printf("INIT PDCP %p RRC %p\n", &pdcp, &rrc);
  rlc.init(&pdcp, &rrc, &mac, &timers, rlc_log);
  pdcp.init(&rlc, &rrc, nullptr /* &gtpu */);
  rrc.init(rrc_cfg /*, phy */, &mac, &rlc, &pdcp, &s1ap, /* &gtpu, */ &timers);
//  rrc.init(rrc_cfg /*, phy */, &mac, &rlc,/* &pdcp, * /* &gtpu, */ &timers);
  if (s1ap.init(args.s1ap, &rrc, &timers, this) != SRSLTE_SUCCESS) {
    stack_log->error("Couldn't initialize S1AP\n");
    printf("MAIN: ------------------------------ Couldn't initialize S1AP\n");
    return SRSLTE_ERROR;
  }
  // if (gtpu.init(args.s1ap.gtp_bind_addr,
  //               args.s1ap.mme_addr,
  //               args.embms.m1u_multiaddr,
  //               args.embms.m1u_if_addr,
  //               &pdcp,
  //               this,
  //               args.embms.enable)) {
  //   stack_log->error("Couldn't initialize GTPU\n");
  //   return SRSLTE_ERROR;
  // }

  started = true;
  start(STACK_MAIN_THREAD_PRIO);

  return SRSLTE_SUCCESS;
}

void enb_stack_nb::tti_clock()
{
  pending_tasks.push(sync_queue_id, [this]() { tti_clock_impl(); });
}

void enb_stack_nb::tti_clock_impl()
{
  for (auto& t : deferred_stack_tasks) {
    t();
  }
  deferred_stack_tasks.clear();
  timers.step_all();
  rrc.tti_clock();
}

void enb_stack_nb::stop()
{
  if (started) {
    pending_tasks.push(enb_queue_id, [this]() { stop_impl(); });
    wait_thread_finish();
  }
}

void enb_stack_nb::stop_impl()
{
  rx_sockets->stop();

  // s1ap.stop();
  // gtpu.stop();
   mac.stop();
   rlc.stop();
  pdcp.stop();
  rrc.stop();

  if (args.mac_pcap.enable) {
    mac_pcap.close();
  }
  if (args.s1ap_pcap.enable) {
    s1ap_pcap.close();
  }

  // erasing the queues is the last thing, bc we need them to call stop_impl()
  pending_tasks.erase_queue(sync_queue_id);
  pending_tasks.erase_queue(enb_queue_id);
  pending_tasks.erase_queue(mme_queue_id);
  pending_tasks.erase_queue(gtpu_queue_id);
  pending_tasks.erase_queue(mac_queue_id);

  started = false;
}

// bool enb_stack_nb::get_metrics(stack_metrics_t* metrics)
// {
//   // use stack thread to query metrics
//   pending_tasks.try_push(enb_queue_id, [this]() {
//     stack_metrics_t metrics{};
//     mac.get_metrics(metrics.mac);
//     rrc.get_metrics(metrics.rrc);
//     s1ap.get_metrics(metrics.s1ap);
//     pending_stack_metrics.push(metrics);
//   });

//   // wait for result
//   *metrics = pending_stack_metrics.wait_pop();
//   return true;
// }

void enb_stack_nb::run_thread()
{
  while (started) {
    srslte::move_task_t task{};
    if (pending_tasks.wait_pop(&task) >= 0) {
      task();
    }
  }
}

 void enb_stack_nb::handle_mme_rx_packet(srslte::unique_byte_buffer_t pdu,
                                          const sockaddr_in&           from,
                                          const sctp_sndrcvinfo&       sri,
                                          int                          flags)
 {
   // Defer the handling of MME packet to eNB stack main thread
   auto task_handler = [this, from, sri, flags](srslte::unique_byte_buffer_t& t) {
     s1ap.handle_mme_rx_msg(std::move(t), from, sri, flags);
   };
   // Defer the handling of MME packet to main stack thread
   pending_tasks.push(mme_queue_id, std::bind(task_handler, std::move(pdu)));
 }

void enb_stack_nb::add_mme_socket(int fd)
{
  // Pass MME Rx packet handler functor to socket handler to run in socket thread
  auto mme_rx_handler =
      [this](srslte::unique_byte_buffer_t pdu, const sockaddr_in& from, const sctp_sndrcvinfo& sri, int flags) {
        handle_mme_rx_packet(std::move(pdu), from, sri, flags);
      };
  rx_sockets->add_socket_sctp_pdu_handler(fd, mme_rx_handler);
}

void enb_stack_nb::remove_mme_socket(int fd)
{
  rx_sockets->remove_socket(fd);
}

srslte::timer_handler::unique_timer enb_stack_nb::get_unique_timer()
{
  return timers.get_unique_timer();
}

srslte::task_multiqueue::queue_handler enb_stack_nb::make_task_queue()
{
  return pending_tasks.get_queue_handler();
}

void enb_stack_nb::defer_callback(uint32_t duration_ms, std::function<void()> func)
{
  timers.defer_callback(duration_ms, func);
}

void enb_stack_nb::enqueue_background_task(std::function<void(uint32_t)> task)
{
  task(0);
}

void enb_stack_nb::notify_background_task_result(srslte::move_task_t task)
{
  task();
}

void enb_stack_nb::defer_task(srslte::move_task_t task)
{
  deferred_stack_tasks.push_back(std::move(task));
}

} // namespace sonica_enb
