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

#ifndef SRSENB_NB_PHY_COMMON_H
#define SRSENB_NB_PHY_COMMON_H

#include "phy_interfaces.h"
#include "srslte/common/interfaces_common.h"
#include "srslte/common/log.h"
#include "srslte/common/thread_pool.h"
#include "srslte/common/threads.h"
#include "srslte/interfaces/enb_interfaces_nb.h"
// #include "srslte/interfaces/enb_metrics_interface.h"
#include "srslte/interfaces/radio_interfaces.h"
// #include "srslte/phy/channel/channel.h"
#include "srslte/radio/radio.h"
#include <map>
#include <srslte/common/tti_sempahore.h>
#include <string.h>

namespace sonica_enb {

class phy_common
{
public:
  explicit phy_common() = default;

  bool init(const phy_cell_cfg_nb_t& cell_, srslte::radio_interface_phy* radio_handler,
            stack_interface_phy_nb* stack);
  void reset();
  void stop();
  /**
   * TTI transmission semaphore, used for ensuring that PHY workers transmit following start order
   */
  srslte::tti_semaphore<void*> semaphore;

  /**
   * Performs common end worker transmission tasks such as transmission and stack TTI execution
   *
   * @param tx_sem_id Semaphore identifier, the worker thread pointer is used
   * @param buffer baseband IQ sample buffer
   * @param nof_samples number of samples to transmit
   * @param tx_time timestamp to transmit samples
   */
  void worker_end(void* tx_sem_id, srslte::rf_buffer_t& buffer, uint32_t nof_samples, srslte_timestamp_t tx_time);

  // Common objects
  phy_args_t params = {};

  inline uint32_t get_nof_prb()
  {
    return cell.cell.base.nof_prb;
  }
  inline uint32_t get_nof_ports() const
  {
    return cell.cell.nof_ports;
  }

  inline uint32_t get_nof_rf_channels() const
  {
    return cell.cell.nof_ports;
  }

  inline double get_ul_freq_hz() const
  {
    return cell.ul_freq_hz;
  }

  inline double get_dl_freq_hz() const
  {
    return cell.dl_freq_hz;
  }

  inline srslte_nbiot_cell_t get_cell()
  {
    return cell.cell;
  }

  srslte::radio_interface_phy* radio      = nullptr;
  // TODO: stack interface
  stack_interface_phy_nb*      stack      = nullptr;

  // TODO: UE Database
  // phy_ue_db ue_db;

  // Getters and setters for ul grants which need to be shared between workers
  const stack_interface_phy_nb::ul_sched_list_t& get_ul_grants(uint32_t tti);
  void set_ul_grants(uint32_t tti, const stack_interface_phy_nb::ul_sched_list_t& ul_grants);
private:
  phy_cell_cfg_nb_t cell;
  stack_interface_phy_nb::ul_sched_list_t ul_grants[TTIMOD_SZ] = {};
  std::mutex                              grant_mutex          = {};
};

} // namespace sonica_enb

#endif // SRSENB_NB_PHY_COMMON_H