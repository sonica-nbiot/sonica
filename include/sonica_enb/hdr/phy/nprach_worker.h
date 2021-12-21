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

#ifndef SRSENB_NB_NPRACH_WORKER_H
#define SRSENB_NB_NPRACH_WORKER_H

#include "sonica/sonica.h"

#include "phy_interfaces.h"
#include "srslte/common/block_queue.h"
#include "srslte/common/buffer_pool.h"
#include "srslte/common/log.h"
#include "srslte/common/threads.h"
#include "srslte/interfaces/enb_interfaces_nb.h"

namespace sonica_enb {

class nprach_worker : srslte::thread
{
public:
  nprach_worker() : buffer_pool(8), thread("NPRACH_WORKER") { }

  int  init(const srslte_nbiot_cell_t& cell_,
            const phy_args_t&          args_,
            // const srslte_prach_cfg_t& prach_cfg_,
            stack_interface_phy_nb*    mac,
            srslte::log*               log_h,
            int                        priority);
  int  new_tti(uint32_t tti, cf_t* buffer);
  void stop();

private:
  uint32_t nprach_nof_det     = 0;
  uint32_t nprach_indices[12] = {};
  float    prach_offsets[12]  = {};
  float    prach_p2avg[12]    = {};

  srslte_nbiot_cell_t cell    = {};

  // srslte_nprach_cfg_t nprach_cfg = {};
  sonica_nprach_t     nprach     = {};

  const static int sf_buffer_sz = 8 * 1920;
  class sf_buffer
  {
  public:
    sf_buffer() = default;
    void reset()
    {
      nof_samples = 0;
      tti         = 0;
    }
    cf_t     samples[sf_buffer_sz] = {};
    uint32_t nof_samples           = 0;
    uint32_t tti                   = 0;
#ifdef SRSLTE_BUFFER_POOL_LOG_ENABLED
    char debug_name[SRSLTE_BUFFER_POOL_LOG_NAME_LEN];
#endif /* SRSLTE_BUFFER_POOL_LOG_ENABLED */
  };

  srslte::buffer_pool<sf_buffer>  buffer_pool;
  srslte::block_queue<sf_buffer*> pending_buffers;

  bool                    emulate_nprach      = false;
  stack_interface_phy_nb* stack               = nullptr;
  srslte::log*            log_h               = nullptr;
  sf_buffer*              current_buffer      = nullptr;
  bool                    initiated           = false;
  bool                    running             = false;
  uint32_t                nof_sf              = 0;
  uint32_t                sf_cnt              = 0;

  void run_thread() final;
  int  run_tti(sf_buffer* b);
};

} // namespace sonica_enb

#endif // SRSENB_NB_NPRACH_WORKER_H