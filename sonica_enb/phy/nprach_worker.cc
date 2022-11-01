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

#include "sonica_enb/hdr/phy/nprach_worker.h"
#include "srslte/srslte.h"

namespace sonica_enb {

int nprach_worker::init(const srslte_nbiot_cell_t&      cell_,
                        const phy_args_t&               args_,
                        // const srslte_prach_cfg_t&       prach_cfg_,
                        stack_interface_phy_nb*         stack_,
                        srslte::log*                    log_h_,
                        int                             priority)
{
  log_h      = log_h_;
  stack      = stack_;
  // nprach_cfg = prach_cfg_;
  cell       = cell_;
  emulate_nprach = args_.emulate_nprach;

  if (sonica_nprach_init(&nprach)) {
    return -1;
  }

  // if (sonica_nprach_set_cell(&nprach)) {
  //   return -1;
  // }

  start(priority);
  initiated = true;

  sf_cnt = 0;
  return 0;
}

void nprach_worker::stop()
{
  running = false;
  sonica_nprach_free(&nprach);

  pending_buffers.push(nullptr);
  wait_thread_finish();
}

int nprach_worker::new_tti(uint32_t tti_rx, cf_t* buffer_rx)
{
  // TODO: Compute timing of RA according to config, push actual buffer
  // for computation
  if (tti_rx % 160 == 64 || sf_cnt) {
    if (sf_cnt == 0) {
      current_buffer = buffer_pool.allocate();
      if (!current_buffer) {
        log_h->warning("PRACH skipping tti=%d due to lack of available buffers\n", tti_rx);
        return 0;
      }
    }
    if (!current_buffer) {
      log_h->error("PRACH: Expected available current_buffer\n");
      return -1;
    }
    if (current_buffer->nof_samples + SRSLTE_SF_LEN_PRB(1) < sf_buffer_sz) {
      memcpy(&current_buffer->samples[sf_cnt * SRSLTE_SF_LEN_PRB(1)],
             buffer_rx,
             sizeof(cf_t) * SRSLTE_SF_LEN_PRB(1));
      current_buffer->nof_samples += SRSLTE_SF_LEN_PRB(1);
      if (sf_cnt == 0) {
        current_buffer->tti = tti_rx;
      }
    } else {
      log_h->error("PRACH: Not enough space in current_buffer\n");
      return -1;
    }
    sf_cnt++;
    if (sf_cnt == 7) {
      sf_cnt = 0;
      pending_buffers.push(current_buffer);
    }
  }


  return 0;
}

int nprach_worker::run_tti(sf_buffer* b)
{
  // MOCK: invoke 
  /*
  if (tti_rx == 384) {
    
  }
  */
  uint32_t index;

  printf("Detecting %d samples at TTI %d\n", b->nof_samples, b->tti);
  int ret = sonica_nprach_detect(&nprach, b->samples, b->nof_samples, NULL, &index, NULL);
  sonica_nprach_detect_reset(&nprach);

  if (!emulate_nprach) {
    if (ret) {
      stack->rach_detected(b->tti, index, 5);
    }
  } else {
    if (b->tti == 384) {
      stack->rach_detected(b->tti, 41, 5);
    }
  }
  //printf("MOCK: detected RACH at 38.4\n");
  // if (b->tti != 64 && b->tti != 5184) {
  //   stack->rach_detected(b->tti, b->tti + 7, 42, 0);
  // }

  return 0;
}

void nprach_worker::run_thread()
{
  running = true;
  while (running) {
    sf_buffer* b = pending_buffers.wait_pop();
    if (running && b) {
      int ret = run_tti(b);
      b->reset();
      buffer_pool.deallocate(b);
      if (ret) {
        running = false;
      }
    } else {
      running = false;
    }
  }
}

}
