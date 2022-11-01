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

#include <string.h>

#include "sonica_enb/hdr/stack/mac/scheduler.h"
#include "sonica_enb/hdr/stack/mac/scheduler_ue.h"
#include "srslte/common/log_helper.h"
#include "srslte/common/logmap.h"
#include "srslte/mac/pdu.h"
#include "srslte/srslte.h"

/******************************************************
 *                  UE class                          *
 ******************************************************/

namespace sonica_enb {

/******************************************************
 *                 Helper Functions                   *
 ******************************************************/

namespace sched_utils {

//! Obtains TB size *in bytes* for a given MCS and N_{PRB}
uint32_t get_tbs_bytes(uint32_t mcs, uint32_t nof_alloc_prb, bool use_tbs_index_alt, bool is_ul)
{
  int tbs_idx = srslte_ra_tbs_idx_from_mcs(mcs, use_tbs_index_alt, is_ul);
  if (tbs_idx < SRSLTE_SUCCESS) {
    tbs_idx = 0;
  }

  int tbs = srslte_ra_tbs_from_idx((uint32_t)tbs_idx, nof_alloc_prb);
  if (tbs < SRSLTE_SUCCESS) {
    tbs = 0;
  }

  return (uint32_t)tbs / 8U;
}

//! TS 36.321 sec 7.1.2 - MAC PDU subheader is 2 bytes if L<=128 and 3 otherwise
uint32_t get_mac_subheader_sdu_size(uint32_t sdu_bytes)
{
  return sdu_bytes == 0 ? 0 : (sdu_bytes > 128 ? 3 : 2);
}

} // namespace sched_utils

//bool operator==(const sched_interface::ue_cfg_t::cc_cfg_t& lhs, const sched_interface::ue_cfg_t::cc_cfg_t& rhs)
//{
//  return lhs.enb_cc_idx == rhs.enb_cc_idx and lhs.active == rhs.active;
//}

/*******************************************************
 *
 * Initialization and configuration functions
 *
 *******************************************************/

sched_ue::sched_ue() : log_h(srslte::logmap::get("MAC "))
{
  reset();
}

void sched_ue::init(uint16_t rnti_)
{
  rnti             = rnti_;
  Info("SCHED: Added user rnti=0x%x\n", rnti);
}

void sched_ue::set_cfg(const sched_interface::ue_cfg_t& cfg_)
{
//  // for the first configured cc, set it as primary cc
//  if (cfg.supported_cc_list.empty()) {
//    uint32_t primary_cc_idx = 0;
//    if (not cfg_.supported_cc_list.empty()) {
//      primary_cc_idx = cfg_.supported_cc_list[0].enb_cc_idx;
//    } else {
//      Warning("Primary cc idx not provided in scheduler ue_cfg. Defaulting to cc_idx=0\n");
//    }
//    // setup primary cc
//    main_cc_params = &(*cell_params_list)[primary_cc_idx];
//    cell           = main_cc_params->cfg.cell;
//    max_msg3retx   = main_cc_params->cfg.maxharq_msg3tx;
//  }
//
//  // update configuration
//  std::vector<sched::ue_cfg_t::cc_cfg_t> prev_supported_cc_list = std::move(cfg.supported_cc_list);
  cfg                                                           = cfg_;

  // update bearer cfgs
  for (uint32_t i = 0; i < sched_interface::MAX_LC; ++i) {
    set_bearer_cfg_unlocked(i, cfg.ue_bearers[i]);
  }

//  // either add a new carrier, or reconfigure existing one
//  bool scell_activation_state_changed = false;
//  for (uint32_t ue_idx = 0; ue_idx < cfg.supported_cc_list.size(); ++ue_idx) {
//    auto& cc_cfg = cfg.supported_cc_list[ue_idx];
//
//    if (ue_idx >= prev_supported_cc_list.size()) {
//      // New carrier needs to be added
//      carriers.emplace_back(cfg, (*cell_params_list)[cc_cfg.enb_cc_idx], rnti, ue_idx);
//    } else if (cc_cfg.enb_cc_idx != prev_supported_cc_list[ue_idx].enb_cc_idx) {
//      // One carrier was added in the place of another
//      carriers[ue_idx] = sched_ue_carrier{cfg, (*cell_params_list)[cc_cfg.enb_cc_idx], rnti, ue_idx};
//      if (ue_idx == 0) {
//        log_h->info("SCHED: PCell has changed for rnti=0x%x.\n", rnti);
//      }
//    } else {
//      // The SCell internal configuration may have changed
//      carriers[ue_idx].set_cfg(cfg);
//    }
//    scell_activation_state_changed |= carriers[ue_idx].is_active() != cc_cfg.active and ue_idx > 0;
//  }
//  if (scell_activation_state_changed) {
//    pending_ces.emplace_back(srslte::dl_sch_lcid::SCELL_ACTIVATION);
//    log_h->info("SCHED: Enqueueing SCell Activation CMD for rnti=0x%x\n", rnti);
//  }
}

void sched_ue::reset()
{
  cfg                          = {};
  sr                           = false;
  phy_config_dedicated_enabled = false;
  cqi_request_tti              = 0;
  carriers.clear();

//  // erase all bearers
//  for (uint32_t i = 0; i < cfg.ue_bearers.size(); ++i) {
//    set_bearer_cfg_unlocked(i, {});
//  }
}
/*******************************************************
 *
 * FAPI-like main scheduler interface.
 *
 *******************************************************/

void sched_ue::set_sr()
{
  sr = true;
}

void sched_ue::unset_sr()
{
  sr = false;
}

void sched_ue::dl_buffer_state(uint8_t lc_id, uint32_t tx_queue, uint32_t retx_queue)
{
  if (lc_id < sched_interface::MAX_LC) {
    lch[lc_id].buf_retx = retx_queue;
    lch[lc_id].buf_tx   = tx_queue;
    Debug("SCHED: DL lcid=%d buffer_state=%d,%d\n", lc_id, tx_queue, retx_queue);
    printf("MAC sched_ue::dl_buffer_state : DL lcid=%d buffer_state=%d,%d\n", lc_id, tx_queue, retx_queue);
  }
}

void sched_ue::mac_buffer_state(uint32_t ce_code, uint32_t nof_cmds)
{
  auto cmd = (ce_cmd)ce_code;
  for (uint32_t i = 0; i < nof_cmds; ++i) {
    if (cmd == ce_cmd::CON_RES_ID) {
      pending_ces.push_front(cmd);
    } else {
      pending_ces.push_back(cmd);
    }
  }
  Info("SCHED: %s for rnti=0x%x needs to be scheduled\n", to_string(cmd), rnti);
  printf("SCHED: %s for rnti=0x%x needs to be scheduled\n", to_string(cmd), rnti);
}

void sched_ue::ul_buffer_state(uint8_t lc_id, uint32_t bsr, bool set_value)
{
  if (lc_id < sched_interface::MAX_LC) {
    if (set_value) {
      lch[lc_id].bsr = bsr;
    } else {
      lch[lc_id].bsr += bsr;
    }
  }
  Debug("SCHED: bsr=%d, lcid=%d, bsr={%d,%d,%d,%d}\n", bsr, lc_id, lch[0].bsr, lch[1].bsr, lch[2].bsr, lch[3].bsr);
  printf("MAC SCHED sched_ue::ul_buffer_state: bsr=%d, lcid=%d, bsr={%d,%d,%d,%d}\n", bsr, lc_id, lch[0].bsr, lch[1].bsr, lch[2].bsr, lch[3].bsr);
}

void sched_ue::ul_wait_timer(uint32_t wait_time, bool set_value)
{
  if (set_value) {
    msg_wait_timer = wait_time;
  } else {
    msg_wait_timer += wait_time;
  }
  Debug("MAC SCHED: ul_wait_timer=%d\n", msg_wait_timer);
  printf("MAC SCHED: ul_wait_timer=%d\n", msg_wait_timer);
}
/*******************************************************
 *
 * Functions used to generate DCI grants
 *
 *******************************************************/

/// Modulation and TBS index table for NPUSCH with N_sc_RU == 1 according to Table 16.5.1.2-1 in TS 36.213 13.2.0
const int i_mcs_to_i_tbs_npusch[11] = {0, 2, 1, 3, 4, 5, 6, 7, 8, 9, 10};

// Transport Block Size for NPUSCH
// from 3GPP TS 36.213 v13.2.0 table 16.5.1.2-2
const int tbs_table_npusch[13][8] = {{16, 32, 56, 88, 120, 152, 208, 256},
                                     {24, 56, 88, 144, 176, 208, 256, 344},
                                     {32, 72, 144, 176, 208, 256, 328, 424},
                                     {40, 104, 176, 208, 256, 328, 440, 568},
                                     {56, 120, 208, 256, 328, 408, 552, 680},
                                     {72, 144, 224, 328, 424, 504, 680, 872},
                                     {88, 176, 256, 392, 504, 600, 808, 1000},
                                     {104, 224, 328, 472, 584, 712, 1000, 0},
                                     {120, 256, 392, 536, 680, 808, 0, 0},
                                     {136, 296, 456, 616, 776, 936, 0, 0},
                                     {144, 328, 504, 680, 872, 1000, 0, 0},
                                     {176, 376, 584, 776, 1000, 0, 0, 0},
                                     {208, 440, 680, 1000, 0, 0, 0, 0}};

// Transport Block Size for NPDSCH
// Transport Block Size from 3GPP TS 36.213 v13.2.0 table 16.4.1.5.1-1
const int tbs_table_nbiot[13][8] = {{16, 32, 56, 88, 120, 152, 208, 256},
                                    {24, 56, 88, 144, 176, 208, 256, 344},
                                    {32, 72, 144, 176, 208, 256, 328, 424},
                                    {40, 104, 176, 208, 256, 328, 440, 568},
                                    {56, 120, 208, 256, 328, 408, 552, 680},
                                    {72, 144, 224, 328, 424, 504, 680, 0},
                                    {88, 176, 256, 392, 504, 600, 0, 0},
                                    {104, 224, 328, 472, 584, 680, 0, 0},
                                    {120, 256, 392, 536, 680, 0, 0, 0},
                                    {136, 296, 456, 616, 0, 0, 0, 0},
                                    {144, 328, 504, 680, 0, 0, 0, 0},
                                    {176, 376, 584, 0, 0, 0, 0, 0},
                                    {208, 440, 680, 0, 0, 0, 0, 0}};

int sched_ue::generate_formatN0(sched_interface::ul_sched_data_t* data,
                                //                               uint32_t                          tti,
                                //                               uint32_t                          cc_idx,
                                ul_harq_proc::ul_alloc_t          alloc,
                                bool                              needs_pdcch,
                                //                               srslte_dci_location_t             dci_pos,
                                int                               explicit_mcs)
{
  // Future: add the harq related functions later
  //  ul_harq_proc*    h   = get_ul_harq(tti, cc_idx);

  srslte_nbiot_dci_ul_t* dci = &data->dci;

  // Set DCI position
  data->needs_npdcch = needs_pdcch;

  int mcs = (explicit_mcs >= 0) ? explicit_mcs : carriers[0].fixed_mcs_ul;
  int tbs = 0;

  // uint32_t nof_retx;
  uint32_t i_tbs  = 0;
  uint32_t nof_sc = 12; // Currently only support sc=12
  uint32_t i_ru   = (alloc.len == 6) ? 5 : 3;  // Currently only support RU number=4 (MCS0)

  if (mcs >= 0) {
    if (nof_sc == 1) {
      //        assert(dci->i_mcs < 11);
      //        grant->Qm = (dci->i_mcs <= 1) ? 1 : 2;
      i_tbs = i_mcs_to_i_tbs_npusch[mcs];
    } else if (nof_sc > 1) {
      //        assert(mcs <= 12);
      //        grant->Qm = 2;
      i_tbs = mcs;
    }
    tbs = tbs_table_npusch[i_tbs][i_ru];
  }

  data->tbs = tbs;

  if (tbs > 0) {
    dci->rnti         = rnti;
    dci->format       = SRSLTE_DCI_FORMATN0;
    dci->ra_dci.i_mcs = mcs;
    dci->ra_dci.i_ru  = i_ru;
    dci->ra_dci.i_sc  = 18;
  }

  return tbs;
}

int sched_ue::generate_formatN1(sched_interface::dl_sched_data_t* data,
                                uint32_t                          tti,
                                //                               uint32_t                          cc_idx,
                                //                               ul_harq_proc::ul_alloc_t          alloc,
                                bool needs_npdcch,
                                //                               srslte_dci_location_t             dci_pos,
                                uint32_t i_sf)
{
  // Future: add the harq related functions later
  //  ul_harq_proc*    h   = get_ul_harq(tti, cc_idx);

  srslte_ra_nbiot_dl_dci_t* dci = &data->dci;

  uint32_t tbs_bytes = 0;
  int final_mcs = -1;

  uint32_t                      data_size = get_pending_dl_new_data();
  std::pair<uint32_t, uint32_t> req_bytes = get_requested_dl_bytes(0);

  // Currently only support sc=12
  for (uint32_t mcs = 0; mcs <13; mcs++) {
    tbs_bytes = tbs_table_nbiot[mcs][i_sf] / 8;
    if (tbs_bytes > req_bytes.second) {
      final_mcs = mcs;
      break;
    }
  }

  if (final_mcs == -1) {
    // TODO: Change the MCS/i_sf coordination in the future
    printf("MAC sched_ue::generate_formatN1: cannot find suitable MCS to send all pending data\n");

    printf("NB-IoT: MAC sched_ue::generate_formatN1: --------------------- tmp select MCS=12\n");
    final_mcs = 12;
    //    return 0;
  }

  printf("MAC sched_ue::generate_formatN1 allcoate mcs=%d i_sf=%d\n", final_mcs, i_sf);

  data->tbs[0] = tbs_bytes;
  data->tbs[1] = 0;

  uint32_t rem_tbs = tbs_bytes;

  rem_tbs -= allocate_mac_ces(data, rem_tbs, 0);
  rem_tbs -= allocate_mac_sdus(data, rem_tbs, 0);

  /* Allocate DL UE Harq */
  if (rem_tbs != tbs_bytes) {
    //    h->new_tx(user_mask, tb, tti_tx_dl, mcs, tbs, data->dci.location.ncce);
    Debug("SCHED: Alloc DCI format N1 new mcs=%d, tbs=%d\n", final_mcs, tbs_bytes);
    printf("SCHED:sched_ue::generate_formatN1 Alloc DCI format N1 new mcs=%d, tbs=%d\n", final_mcs, tbs_bytes);
  } else {
    //    Warning("SCHED: Failed to allocate DL harq pid=%d\n", h->get_id());
  }

  if (tbs_bytes > 0) {
    dci->mcs_idx         = final_mcs;
    dci->rv_idx         = 0;
    dci->ndi             = true;
    dci->format          = 1; // FormatN1 DCI
    dci->alloc.has_sib1  = false;
    dci->alloc.is_ra     = false;
    dci->alloc.rnti      = rnti;
    dci->alloc.i_delay   = 0;
    dci->alloc.i_sf      = i_sf; // i_sf_val
    dci->alloc.i_rep     = 0;    // i_rep_val
    dci->alloc.harq_ack  = 1;
    dci->alloc.i_n_start = 0;
  }

  return tbs_bytes;
}

constexpr uint32_t min_mac_sdu_size = 5; // accounts for MAC SDU subheader and RLC header

/**
 * Allocate space for multiple MAC SDUs (i.e. RLC PDUs) and corresponding MAC SDU subheaders
 * @param data struct where the rlc pdu allocations are stored
 * @param total_tbs available TB size for allocations for a single UE
 * @param tbidx index of TB
 * @return allocated bytes, which is always equal or lower than total_tbs
 */
uint32_t sched_ue::allocate_mac_sdus(sched_interface::dl_sched_data_t* data, uint32_t total_tbs, uint32_t tbidx)
{
  // TS 36.321 sec 7.1.2 - MAC PDU subheader is 2 bytes if L<=128 and 3 otherwise
  auto     compute_subheader_size = [](uint32_t sdu_size) { return sdu_size > 128 ? 3 : 2; };
  uint32_t rem_tbs                = total_tbs;

  // if we do not have enough bytes to fit MAC subheader and RLC header, skip MAC SDU allocation
  while (rem_tbs >= min_mac_sdu_size) {
    uint32_t max_sdu_bytes   = rem_tbs - compute_subheader_size(rem_tbs - 2);
    uint32_t alloc_sdu_bytes = alloc_rlc_pdu(&data->pdu[tbidx][data->nof_pdu_elems[tbidx]], max_sdu_bytes);
    if (alloc_sdu_bytes == 0) {
      break;
    }
    rem_tbs -= (alloc_sdu_bytes + compute_subheader_size(alloc_sdu_bytes)); // account for MAC sub-header
    data->nof_pdu_elems[tbidx]++;
  }

  return total_tbs - rem_tbs;
}

/**
 * Allocate space for pending MAC CEs
 * @param data struct where the MAC CEs allocations are stored
 * @param total_tbs available space in bytes for allocations
 * @return number of bytes allocated
 */
uint32_t sched_ue::allocate_mac_ces(sched_interface::dl_sched_data_t* data, uint32_t total_tbs, uint32_t ue_cc_idx)
{
  if (ue_cc_idx != 0) {
    return 0;
  }

  int rem_tbs = total_tbs;
  while (not pending_ces.empty()) {
    int toalloc = srslte::ce_total_size(pending_ces.front());
    if (rem_tbs < toalloc) {
      break;
    }
    data->pdu[0][data->nof_pdu_elems[0]].lcid = (uint32_t)pending_ces.front();
    data->nof_pdu_elems[0]++;
    rem_tbs -= toalloc;
    Info("SCHED: Added a MAC %s CE for rnti=0x%x\n", srslte::to_string(pending_ces.front()), rnti);
    // printf("SCHED:sched_ue::allocate_mac_ces Added a MAC %s CE for rnti=0x%x\n", srslte::to_string(pending_ces.front()), rnti);
    pending_ces.pop_front();
  }
  return total_tbs - rem_tbs;
}

/* Allocates first available RLC PDU */
int sched_ue::alloc_rlc_pdu(sched_interface::dl_sched_pdu_t* mac_sdu, int rem_tbs)
{
  // TODO: Implement lcid priority (now lowest index is lowest priority)
  int alloc_bytes = 0;
  int i           = 0;
  for (i = 0; i < sched_interface::MAX_LC and alloc_bytes == 0; i++) {
    if (lch[i].buf_retx > 0) {
      alloc_bytes = SRSLTE_MIN(lch[i].buf_retx, rem_tbs);
      lch[i].buf_retx -= alloc_bytes;
    } else if (lch[i].buf_tx > 0) {
      alloc_bytes = SRSLTE_MIN(lch[i].buf_tx, rem_tbs);
      lch[i].buf_tx -= alloc_bytes;
    }
  }
  if (alloc_bytes > 0) {
    mac_sdu->lcid   = i - 1;
    mac_sdu->nbytes = alloc_bytes;
    Debug("SCHED: Allocated lcid=%d, nbytes=%d, tbs_bytes=%d\n", mac_sdu->lcid, mac_sdu->nbytes, rem_tbs);
    //    printf("SCHED:sched_ue::alloc_rlc_pdu Allocated lcid=%d, nbytes=%d, tbs_bytes=%d\n", mac_sdu->lcid, mac_sdu->nbytes, rem_tbs);
  }
  return alloc_bytes;
}

/*******************************************************
 *
 * Functions used by scheduler or scheduler metric objects
 *
 *******************************************************/

bool sched_ue::bearer_is_ul(const ue_bearer_t* lch)
{
  return lch->cfg.direction == sched_interface::ue_bearer_cfg_t::UL ||
  lch->cfg.direction == sched_interface::ue_bearer_cfg_t::BOTH;
}

bool sched_ue::bearer_is_dl(const ue_bearer_t* lch)
{
  return lch->cfg.direction == sched_interface::ue_bearer_cfg_t::DL ||
  lch->cfg.direction == sched_interface::ue_bearer_cfg_t::BOTH;
}

/**
 * Returns the range (min,max) of possible MAC PDU sizes.
 * - the lower boundary value is set based on the following conditions:
 *   - if there is data in SRB0, the min value is the sum of:
 *     - SRB0 RLC data (Msg4) including MAC subheader and payload (no segmentation)
 *     - ConRes CE + MAC subheader (7 bytes)
 *   - elif there is data in other RBs, the min value is either:
 *     - first pending CE (subheader+CE payload) in queue, if it exists and we are in PCell. Or,
 *     - one subheader (2B) + one RLC header (<=3B) to allow one MAC PDU alloc
 * - the upper boundary is set as a sum of:
 *   - total data in all SRBs and DRBs including the MAC subheaders
 *   - All CEs (ConRes and others) including respective MAC subheaders
 * @ue_cc_idx carrier where allocation is being made
 * @return
 */
std::pair<uint32_t, uint32_t> sched_ue::get_requested_dl_bytes(uint32_t ue_cc_idx)
{
  const uint32_t min_alloc_bytes = 5; // 2 for subheader, and 3 for RLC header
  // Convenience function to compute the number of bytes allocated for a given SDU
  auto compute_sdu_total_bytes = [&min_alloc_bytes](uint32_t lcid, uint32_t buffer_bytes) {
    if (buffer_bytes == 0) {
      return 0u;
    }
    uint32_t subheader_and_sdu = buffer_bytes + sched_utils::get_mac_subheader_sdu_size(buffer_bytes);
    return (lcid == 0) ? subheader_and_sdu : std::max(subheader_and_sdu, min_alloc_bytes);
  };

  /* Set Maximum boundary */
  // Ensure there is space for ConRes and RRC Setup
  // SRB0 is a special case due to being RLC TM (no segmentation possible)
  if (not bearer_is_dl(&lch[0])) {
    log_h->error("SRB0 must always be activated for DL\n");
    //    printf("MAC SRB0 must always be activated for DL\n");
    return {0, 0};
  }
  //  if (not carriers[ue_cc_idx].is_active()) {
  //    return {0, 0};
  //  }

  uint32_t max_data = 0, min_data = 0;
  uint32_t srb0_data = 0, rb_data = 0, sum_ce_data = 0;
  //  bool     is_dci_format1 = get_dci_format() == SRSLTE_DCI_FORMAT1;
  bool is_dci_format1 = true;
  if (is_dci_format1 and (lch[0].buf_tx > 0 or lch[0].buf_retx > 0)) {
     printf("MAC sched_ue::get_requested_dl_bytes buf_tx=%d, buf_retx=%d\n", lch[0].buf_tx, lch[0].buf_retx);
    srb0_data = compute_sdu_total_bytes(0, lch[0].buf_retx);
    srb0_data += compute_sdu_total_bytes(0, lch[0].buf_tx);
  }
  // Add pending CEs
  if (ue_cc_idx == 0) {
    for (const auto& ce : pending_ces) {
      // printf("MAC ---- sched_ue::get_requested_dl_bytes add pending_ce\n");
      sum_ce_data += srslte::ce_total_size(ce);
    }
  }
  // Add pending data in remaining RLC buffers
  for (int i = 1; i < sched_interface::MAX_LC; i++) {
    if (bearer_is_dl(&lch[i])) {
      if (lch[i].buf_tx > 0 or lch[i].buf_retx > 0){
        printf("MAC sched_ue::get_requested_dl_bytes lcid=%d, buf_tx=%d, buf_retx=%d\n", i, lch[i].buf_tx, lch[i].buf_retx);
      }
      rb_data += compute_sdu_total_bytes(i, lch[i].buf_retx);
      rb_data += compute_sdu_total_bytes(i, lch[i].buf_tx);
    }
  }
  max_data = srb0_data + sum_ce_data + rb_data;
  // printf("MAC ---- sched_ue::get_requested_dl_bytes 2 srb0_data=%d, sum_ce_data=%d,
  // rb_data=%d\n",srb0_data,sum_ce_data,rb_data);

  /* Set Minimum boundary */
  min_data = srb0_data;
  if (not pending_ces.empty() and pending_ces.front() == ce_cmd::CON_RES_ID) {
    min_data += srslte::ce_total_size(pending_ces.front());
  }
  if (min_data == 0) {
    if (sum_ce_data > 0) {
      min_data = srslte::ce_total_size(pending_ces.front());
    } else if (rb_data > 0) {
      min_data = min_alloc_bytes;
    }
  }

  // printf("MAC ---- sched_ue::get_requested_dl_bytes buf_tx=%d, buf_retx=%d, min_data=%d, max_data=%d\n",
  // lch[0].buf_tx, lch[0].buf_retx, min_data, max_data);
  return {min_data, max_data};
}

/**
 * Get pending DL data in RLC buffers + CEs
 * @return
 */
uint32_t sched_ue::get_pending_dl_new_data()
{
  //  if (std::count_if(carriers.begin(), carriers.end(), [](const sched_ue_carrier& cc) { return cc.is_active(); }) ==
  //  0) {
  //    return 0;
  //  }

  uint32_t pending_data = 0;
  for (int i = 0; i < sched_interface::MAX_LC; i++) {
    if (bearer_is_dl(&lch[i])) {
      pending_data += lch[i].buf_retx + lch[i].buf_tx;
    }
  }
  for (auto& ce : pending_ces) {
    pending_data += srslte::ce_total_size(ce);
  }
  return pending_data;
}

uint32_t sched_ue::get_pending_ul_new_data(uint32_t tti)
{
  return get_pending_ul_new_data_unlocked(tti);
}

uint32_t sched_ue::get_pending_ul_old_data(uint32_t cc_idx)
{
  return get_pending_ul_old_data_unlocked(cc_idx);
}

// Private lock-free implementation
uint32_t sched_ue::get_pending_ul_new_data_unlocked(uint32_t tti)
{
  uint32_t pending_data = 0;
  for (int i = 0; i < sched_interface::MAX_LC; i++) {
    if (bearer_is_ul(&lch[i])) {
      pending_data += lch[i].bsr;
    }
  }
  if (pending_data == 0 and is_sr_triggered()) {
    return 125;
  }

  // Subtract all the UL data already allocated in the UL harqs
  uint32_t pending_ul_data = 0;
  for (uint32_t cc_idx = 0; cc_idx < carriers.size(); ++cc_idx) {
    pending_ul_data += get_pending_ul_old_data_unlocked(cc_idx);
  }
  pending_data = (pending_data > pending_ul_data) ? pending_data - pending_ul_data : 0;

  if (pending_data > 0) {
    Debug("SCHED: pending_data=%d, pending_ul_data=%d, bsr={%d,%d,%d,%d}\n",
          pending_data,
          pending_ul_data,
          lch[0].bsr,
          lch[1].bsr,
          lch[2].bsr,
          lch[3].bsr);
  }
  return pending_data;
}

// Private lock-free implementation
uint32_t sched_ue::get_pending_ul_old_data_unlocked(uint32_t cc_idx)
{
  uint32_t pending_data = 0;
  //  for (auto& h : carriers[cc_idx].harq_ent.ul_harq_procs()) {
  //    pending_data += h.get_pending_data();
  //  }
  return pending_data;
}

void sched_ue::set_bearer_cfg_unlocked(uint32_t lc_id, const sched_interface::ue_bearer_cfg_t& cfg_)
{
  if (lc_id < sched_interface::MAX_LC) {

    bool is_idle   = lch[lc_id].cfg.direction == sched_interface::ue_bearer_cfg_t::IDLE;
    bool is_equal  = memcmp(&cfg_, &lch[lc_id].cfg, sizeof(cfg_)) == 0;
    lch[lc_id].cfg = cfg_;
    //    printf("SCHED: sched_ue::set_bearer_cfg lcid=%d, is_idle=%d, is_equal=%d\n", lc_id, is_idle, is_equal);
    if (lch[lc_id].cfg.direction != sched_interface::ue_bearer_cfg_t::IDLE) {
      if (not is_equal) {
        Info("SCHED: Set bearer config lc_id=%d, direction=%d\n", lc_id, (int)lch[lc_id].cfg.direction);
        printf("SCHED: Set bearer config lc_id=%d, direction=%d\n", lc_id, (int)lch[lc_id].cfg.direction);
      }
    } else if (not is_idle) {
      Info("SCHED: Removed bearer config lc_id=%d, direction=%d\n", lc_id, (int)lch[lc_id].cfg.direction);
      printf("SCHED: Removed bearer config lc_id=%d, direction=%d\n", lc_id, (int)lch[lc_id].cfg.direction);
    }
  }
}

bool sched_ue::is_sr_triggered()
{
  return sr;
}

} // namespace sonica_enb
