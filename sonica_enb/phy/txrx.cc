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

#include <unistd.h>

#include "srslte/common/log.h"
#include "srslte/common/threads.h"
#include "srslte/srslte.h"

#include "sonica_enb/hdr/phy/sf_worker.h"
#include "sonica_enb/hdr/phy/txrx.h"

#define Error(fmt, ...)                                                                                                \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...)                                                                                              \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...)                                                                                                 \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)                                                                                                \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->debug(fmt, ##__VA_ARGS__)

using namespace std;

namespace sonica_enb {


txrx::txrx() : thread("TXRX")
{
  /* Do nothing */
}

bool txrx::init(srslte::radio_interface_phy* radio_h_,
                srslte::thread_pool*         workers_pool_,
                phy_common*                  worker_com_,
                nprach_worker*               nprach_,
                srslte::log*                 log_h_,
                uint32_t                     prio_)
{
  radio_h       = radio_h_;
  log_h         = log_h_;
  workers_pool  = workers_pool_;
  worker_com    = worker_com_;
  nprach        = nprach_;
  tx_worker_cnt = 0;
  running       = true;

  nof_workers = workers_pool->get_nof_workers();

  start(prio_);
  return true;
}

void txrx::stop()
{
  if (running) {
    running = false;
    wait_thread_finish();
  }
}

void txrx::run_thread()
{
  sf_worker*          worker  = nullptr;
  srslte::rf_buffer_t buffer  = {};
  srslte_timestamp_t  rx_time = {};
  srslte_timestamp_t  tx_time = {};
  uint32_t            sf_len  = SRSLTE_SF_LEN_PRB(worker_com->get_nof_prb());

  float samp_rate = srslte_sampling_freq_hz(worker_com->get_nof_prb());

  // Configure radio
  radio_h->set_rx_srate(samp_rate);
  radio_h->set_tx_srate(samp_rate);

  // Set Tx/Rx frequencies
  double   tx_freq_hz = worker_com->get_dl_freq_hz();
  double   rx_freq_hz = worker_com->get_ul_freq_hz();
  log_h->console(
      "Setting frequency: DL=%.4f Mhz, UL=%.4f MHz", tx_freq_hz / 1e6f, rx_freq_hz / 1e6f);
  radio_h->set_tx_freq(0, tx_freq_hz);
  radio_h->set_rx_freq(0, rx_freq_hz);

  log_h->info("Starting RX/TX thread nof_prb=%d, sf_len=%d\n", worker_com->get_nof_prb(), sf_len);

  // Set TTI so that first TX is at tti=0
  tti = TTI_SUB(0, FDD_HARQ_DELAY_UL_MS + 1);
  hfn = 1023;

  // Main loop
  while (running) {
    // XXX: Refine this messy logic
    // The HFN passed to SF worker is the Tx HFN
    uint32_t tx_hfn = hfn;
    if (tti >= 10240 - FDD_HARQ_DELAY_UL_MS - 1) {
      tx_hfn = (tx_hfn + 1) % 1024;
      if (tti >= 10240 - 1) {
        hfn = tx_hfn;
      }
    }

    tti    = TTI_ADD(tti, 1);
    worker = (sf_worker*)workers_pool->wait_worker(tti);

    if (worker) {
      // Multiple cell buffer mapping

      for (uint32_t p = 0; p < worker_com->get_nof_ports(); p++) {
        // WARNING: The number of ports for all cells must be the same
        buffer.set(0, p, worker_com->get_nof_ports(), worker->get_buffer_rx(p));
      }

      radio_h->rx_now(buffer, sf_len, &rx_time);

      /* Compute TX time: Any transmission happens in TTI+4 thus advance 4 ms the reception time */
      srslte_timestamp_copy(&tx_time, &rx_time);
      srslte_timestamp_add(&tx_time, 0, FDD_HARQ_DELAY_UL_MS * 1e-3);

      Debug("Setting TTI=%d, tx_mutex=%d, tx_time=%ld:%f to worker %d\n",
            tti,
            tx_worker_cnt,
            tx_time.full_secs,
            tx_time.frac_secs,
            worker->get_id());

      worker->set_time(tx_hfn, tti, tx_worker_cnt, tx_time);
      tx_worker_cnt = (tx_worker_cnt + 1) % nof_workers;

      // Trigger phy worker execution
      worker_com->semaphore.push(worker);
      workers_pool->start_worker(worker);

      // Trigger prach worker execution
      nprach->new_tti(tti, buffer.get(0, 0, worker_com->get_nof_ports()));
    } else {
      // wait_worker() only returns NULL if it's being closed. Quit now to avoid unnecessary loops here
      running = false;
    }
  }
}

} // namespace sonica_enb