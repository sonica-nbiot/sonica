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

#include <sonica_enb/hdr/stack/mac/scheduler_ue.h>
#include <string.h>

#include "sonica_enb/hdr/stack/mac/scheduler.h"
#include "sonica_enb/hdr/stack/mac/scheduler_carrier.h"
#include "srslte/common/logmap.h"
#include "srslte/srslte.h"

#define Console(fmt, ...) srslte::logmap::get("MAC ")->console(fmt, ##__VA_ARGS__)
#define Error(fmt, ...) srslte::logmap::get("MAC ")->error(fmt, ##__VA_ARGS__)

namespace sonica_enb {

namespace sched_utils {

uint32_t tti_subtract(uint32_t tti1, uint32_t tti2)
{
  return TTI_SUB(tti1, tti2);
}

uint32_t max_tti(uint32_t tti1, uint32_t tti2)
{
  return ((tti1 - tti2) > 10240 / 2) ? SRSLTE_MIN(tti1, tti2) : SRSLTE_MAX(tti1, tti2);
}

} // namespace sched_utils

/*******************************************************
 *
 * Initialization and sched configuration functions
 *
 *******************************************************/

sched::sched() : log_h(srslte::logmap::get("MAC"))
{}

sched::~sched()
{}

void sched::init()
{
//  rrc = rrc_;
//
//  // Initialize first carrier scheduler
  carrier_schedulers.emplace_back(new carrier_sched{&ue_db});

  reset();

  // XXX (Boyan): This should be invoked by RRC
  cell_cfg();
}

int sched::reset()
{
  std::lock_guard<std::mutex> lock(sched_mutex);
//  configured = false;
//  for (std::unique_ptr<carrier_sched>& c : carrier_schedulers) {
//    c->reset();
//  }
  ue_db.clear();
  return 0;
}

int sched::dl_rach_info(dl_sched_rar_info_t rar_info)
{
  std::lock_guard<std::mutex> lock(sched_mutex);
  return carrier_schedulers[0]->dl_rach_info(rar_info);
}

int sched::cell_cfg()
{
  carrier_schedulers[0]->carrier_cfg();

  return 0;
}

/*******************************************************
 *
 * FAPI-like main sched interface. Wrappers to UE object
 *
 *******************************************************/

int sched::ue_cfg(uint16_t rnti, const sched_interface::ue_cfg_t& ue_cfg)
{
  std::lock_guard<std::mutex> lock(sched_mutex);
  // Add or config user
  auto it = ue_db.find(rnti);
  if (it == ue_db.end()) {
    // create new user
    ue_db[rnti].init(rnti);
    it = ue_db.find(rnti);
  }
  it->second.set_cfg(ue_cfg);

  return SRSLTE_SUCCESS;
}

int sched::ue_rem(uint16_t rnti)
{
  std::lock_guard<std::mutex> lock(sched_mutex);
  if (ue_db.count(rnti) > 0) {
    ue_db.erase(rnti);
  } else {
    Error("User rnti=0x%x not found\n", rnti);
    return SRSLTE_ERROR;
  }
  return SRSLTE_SUCCESS;
}

bool sched::ue_exists(uint16_t rnti)
{
  return ue_db_access(rnti, [](sched_ue& ue) {}) >= 0;
}

int sched::dl_rlc_buffer_state(uint16_t rnti, uint32_t lc_id, uint32_t tx_queue, uint32_t retx_queue)
{
  return ue_db_access(rnti, [&](sched_ue& ue) { ue.dl_buffer_state(lc_id, tx_queue, retx_queue); });
}

int sched::dl_mac_buffer_state(uint16_t rnti, uint32_t ce_code, uint32_t nof_cmds)
{
  return ue_db_access(rnti, [ce_code, nof_cmds](sched_ue& ue) { ue.mac_buffer_state(ce_code, nof_cmds); });
}

int sched::ul_bsr(uint16_t rnti, uint32_t lcid, uint32_t bsr, bool set_value)
{
  return ue_db_access(rnti, [lcid, bsr, set_value](sched_ue& ue) { ue.ul_buffer_state(lcid, bsr, set_value); });
}

int sched::ul_wait_timer(uint16_t rnti, uint32_t wait_time, bool set_value)
{
  return ue_db_access(rnti, [wait_time, set_value](sched_ue& ue) { ue.ul_wait_timer(wait_time, set_value); });
}

/*******************************************************
 *
 * Main sched functions
 *
 *******************************************************/

// Downlink Scheduler API
int sched::dl_sched(uint32_t hfn, uint32_t tti_tx_dl, sched_interface::dl_sched_res_t &sched_result)
{
  if (!configured) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(sched_mutex);
  uint32_t tti_rx = sched_utils::tti_subtract(tti_tx_dl, FDD_HARQ_DELAY_UL_MS);
  last_tti = sched_utils::max_tti(last_tti, tti_rx);

  if (carrier_schedulers.size() > 0) {
    // Compute scheduling Result for tti_rx
    const sf_sched_result &tti_sched = carrier_schedulers[0]->generate_tti_result(hfn,tti_rx, true);

    // copy result
    sched_result = tti_sched.dl_sched_result;
  }

  return 0;
}

// Uplink Scheduler API
int sched::ul_sched(uint32_t hfn, uint32_t tti, sonica_enb::sched_interface::ul_sched_res_t& sched_result)
{
  if (!configured) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(sched_mutex);
  // Compute scheduling Result for tti_rx
  uint32_t tti_rx = sched_utils::tti_subtract(tti, FDD_DELAY_UL_NB_MS + FDD_HARQ_DELAY_DL_MS);

    const sf_sched_result& tti_sched = carrier_schedulers[0]->generate_tti_result(hfn,tti_rx, false);

    // copy result
    sched_result = tti_sched.ul_sched_result;


  return SRSLTE_SUCCESS;
}

// Common way to access ue_db elements in a read locking way
template <typename Func>
int sched::ue_db_access(uint16_t rnti, Func f, const char* func_name)
{
  std::lock_guard<std::mutex> lock(sched_mutex);
  auto                        it = ue_db.find(rnti);
  if (it != ue_db.end()) {
    f(it->second);
  } else {
    if (func_name != nullptr) {
      Error("User rnti=0x%x not found. Failed to call %s.\n", rnti, func_name);
    } else {
      Error("User rnti=0x%x not found.\n", rnti);
    }
    return SRSLTE_ERROR;
  }
  return SRSLTE_SUCCESS;
}


/*******************************************************
 *
 * Helper functions and common data types
 *
 *******************************************************/

void sched_cell_params_t::regs_deleter::operator()(srslte_regs_t *p)
{
  if (p != nullptr) {
    srslte_regs_free(p);
    delete p;
  }
}

rbg_range_t rbg_range_t::prbs_to_rbgs(const prb_range_t &prbs, uint32_t P)
{
  return rbg_range_t{srslte::ceil_div(prbs.prb_min, P), srslte::ceil_div(prbs.prb_min, P)};
}

prb_range_t prb_range_t::rbgs_to_prbs(const rbg_range_t &rbgs, uint32_t P)
{
  return prb_range_t{rbgs.rbg_min * P, rbgs.rbg_max * P};
}

prb_range_t prb_range_t::riv_to_prbs(uint32_t riv, uint32_t nof_prbs, int nof_vrbs)
{
  prb_range_t p;
  if (nof_vrbs < 0) {
    nof_vrbs = nof_prbs;
  }
  srslte_ra_type2_from_riv(riv, &p.prb_max, &p.prb_min, nof_prbs, (uint32_t) nof_vrbs);
  p.prb_max += p.prb_min;
  return p;
}

} // namespace sonica_enb
