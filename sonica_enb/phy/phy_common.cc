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

#include "sonica_enb/hdr/phy/phy_common.h"
#include "sonica_enb/hdr/phy/txrx.h"
#include "srslte/asn1/rrc_asn1.h"
#include "srslte/common/log.h"
#include "srslte/common/threads.h"
#include <sstream>

#include <assert.h>

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
using namespace asn1::rrc;

namespace sonica_enb {


void phy_common::reset()
{
  for (auto& q : ul_grants) {
    for (auto& g : q) {
      g = {};
    }
  }
}

bool phy_common::init(const phy_cell_cfg_nb_t&     cell_,
                      srslte::radio_interface_phy* radio_h_,
                      stack_interface_phy_nb*      stack_)
{
  radio = radio_h_;
  cell  = cell_;
  stack = stack_;

  // TODO: Set UE PHY data-base stack and configuration
  // ue_db.init(stack, params, cell_list);

  // Create grants
  for (auto& q : ul_grants) {
    q.resize(1);
  }

  reset();
  return true;
}

void phy_common::stop()
{
  semaphore.wait_all();
}

const stack_interface_phy_nb::ul_sched_list_t& phy_common::get_ul_grants(uint32_t tti)
{
  std::lock_guard<std::mutex> lock(grant_mutex);
  return ul_grants[tti % TTIMOD_SZ];
}

void phy_common::set_ul_grants(uint32_t tti, const stack_interface_phy_nb::ul_sched_list_t& ul_grant_list)
{
  std::lock_guard<std::mutex> lock(grant_mutex);
  ul_grants[tti % TTIMOD_SZ] = ul_grant_list;
}

/* The transmission of UL subframes must be in sequence. The correct sequence is guaranteed by a chain of N semaphores,
 * one per TTI%nof_workers. Each threads waits for the semaphore for the current thread and after transmission allows
 * next TTI to be transmitted
 *
 * Each worker uses this function to indicate that all processing is done and data is ready for transmission or
 * there is no transmission at all (tx_enable). In that case, the end of burst message will be sent to the radio
 */
void phy_common::worker_end(void*                tx_sem_id,
                            srslte::rf_buffer_t& buffer,
                            uint32_t             nof_samples,
                            srslte_timestamp_t   tx_time)
{
  // Wait for the green light to transmit in the current TTI
  semaphore.wait(tx_sem_id);

  // Always transmit on single radio
  radio->tx(buffer, nof_samples, tx_time);

  // Trigger MAC clock
   stack->tti_clock();

  // Allow next TTI to transmit
  semaphore.release();
}

} // namespace sonica_enb