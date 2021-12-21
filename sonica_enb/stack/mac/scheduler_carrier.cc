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

#include "sonica_enb/hdr/stack/mac/scheduler_carrier.h"
#include "srslte/common/log_helper.h"
#include "srslte/common/logmap.h"

namespace sonica_enb {

/*******************************************************
 *        Broadcast (SIB+Paging) scheduling
 *******************************************************/

bc_sched::bc_sched() : log_h(srslte::logmap::get("MAC"))
{
  // TODO: set MIB parameters according to cell config
  mib_nb.sched_info_sib1 = 5; // sched_info_tag;
  mib_nb.sys_info_tag    = 14;
  mib_nb.ac_barring      = false;
  mib_nb.mode            = SRSLTE_NBIOT_MODE_STANDALONE;

  uint32_t num_rep    = srslte_ra_n_rep_sib1_nb(&mib_nb);
  uint32_t sib1_start = srslte_ra_nbiot_get_starting_sib1_frame(99, &mib_nb); // FIXME: Set tmp cell.n_id_ncell=99
  uint32_t idx        = 0;

  for (uint32_t k = 0; k < 4; k++) {
    // in each period, the SIB is transmitted nrep-times over 16 consecutive frames in every second frame
    for (uint32_t i = 0; i < num_rep; i++) {
      sib1_sfn[idx] = k * SIB1_NB_TTI + sib1_start + (i * SIB1_NB_TTI / num_rep);
      idx++;
    }
  }

  if (!(idx <= 4 * SIB1_NB_MAX_REP && idx == 4 * num_rep)) {
    log_h->error("BC SCHED: (idx <= 4 * SIB1_NB_MAX_REP && idx == 4 * num_rep) Error\n");
    //log_h->console("BC SCHED: (idx <= 4 * SIB1_NB_MAX_REP && idx == 4 * num_rep) Error\n");
  }

  ra_dl_sib1.alloc.has_sib1        = true;
  ra_dl_sib1.alloc.rnti            = SRSLTE_SIRNTI;
  ra_dl_sib1.alloc.sched_info_sib1 = mib_nb.sched_info_sib1;
  ra_dl_sib1.mcs_idx               = 1; // This MCS doesn't seem to matter
  sib1_num_sf                      = 8 * srslte_ra_n_rep_from_dci(&ra_dl_sib1);
  sib1_sf_idx                      = 0;

  ra_dl_sib2.alloc.has_sib1 = false;
  ra_dl_sib2.alloc.rnti     = SRSLTE_SIRNTI;
  ra_dl_sib2.alloc.i_sf     = 6;
  // TODO: Set MCS and SF according to TBS configuration in RRC
  ra_dl_sib2.mcs_idx        = 3;
  sib2_num_sf               = 8;
}

void bc_sched::dl_sched(uint32_t hfn, sf_sched* tti_sched)
{
  current_tti = tti_sched->get_tti_tx_dl();
  current_hfn = hfn;

  /* Allocate SIB */
  alloc_sibs(hfn, tti_sched);

  //  /* Allocate Paging */
  //  // NOTE: It blocks
  //  alloc_paging(tti_sched);
}

bool bc_sched::is_sib1_sf(uint32_t sfn, uint32_t sf_idx)
{
  bool ret = false;
  if (sf_idx != 4)
    return ret;

  for (int i = 0; i < SIB1_NB_MAX_REP * 4; i++) {
    // every second frame within the next 16 SFN starting at q->sib1_sfn[i] are valid
    uint32_t valid_si_sfn = sfn + srslte_ra_nbiot_sib1_start(99, &mib_nb); // FIXME: Set tmp cell.n_id_ncell=99
    if ((sfn >= sib1_sfn[i]) && (sfn < sib1_sfn[i] + 16) && (valid_si_sfn % 2 == 0)) {
      ret = true;
      break;
    }
  }

  return ret;
}

void bc_sched::alloc_sibs(uint32_t hfn, sf_sched* tti_sched)
{
  uint32_t sf_idx = current_tti % 10;
  uint32_t sfn    = current_tti / 10;

  if (is_sib1_sf(sfn, sf_idx)) {

    // Check the sched table
    if (tti_sched->dl_sched_table[current_tti]) {
      log_h->error("BC SCHED Error: SIB1 TTI %d has been allocated!\n", current_tti);
      //printf("BC SCHED Error: SIB1 TTI %d has been allocated!\n", current_tti);
      return;
    } else {
      log_h->error("BC SCHED: SIB1 TTI %d is allocated!\n", current_tti);
    }

    sf_sched::bc_alloc_t bc_alloc{};

    if (sib1_sf_idx == 0) {
      bc_alloc.is_new_sib = true;
    } else {
      bc_alloc.is_new_sib = false;
    }

    // Update the sched table
    tti_sched->dl_sched_table[current_tti] = true;
    //printf("DL_sched_table %d: allocate to sib1\n", current_tti);

    // Increase the sf_idx
    sib1_sf_idx++;

    bc_alloc.dci = ra_dl_sib1;
    bc_alloc.alloc_type = alloc_type_t::DL_BC;
    tti_sched->bc_allocs.emplace_back(bc_alloc);

    if (sib1_sf_idx == sib1_num_sf) {
      sib1_sf_idx = 0;
    }
  } else if (sf_idx != 0 && sf_idx != 5 && !(sf_idx == 9 && sfn % 2 == 0)) {
    // Check SIB2 TTI
    if (sf_idx == 1 && sfn % 512 < 16 && sfn % 4 == 0) {

      // Check the sched table
      if (tti_sched->dl_sched_table[current_tti]) {
        log_h->error("BC SCHED Error: SIB2 TTI has been allocated!\n");
        //printf("BC SCHED Error: SIB2 TTI %d has been allocated!\n", current_tti);
        return;
      }

      //log_h->console("MAC SIB2 %d/%d.%d\n", hfn, sfn, sf_idx);

      // Update dl_sched table
      uint32_t alloc_i = 0, rem_sib = sib2_num_sf;
      while (rem_sib > 0) {
        if (srslte_ra_nbiot_is_valid_dl_sf((current_tti + alloc_i) % 10240)) {
          //printf("DL_sched_table %d: allocate to sib2\n", (current_tti + alloc_i) % 10240);
          tti_sched->dl_sched_table[(current_tti + alloc_i) % 10240] = true;
          rem_sib--;
        }
        alloc_i++;
      }

      sf_sched::bc_alloc_t bc_alloc{};
      bc_alloc.dci = ra_dl_sib2;
      bc_alloc.alloc_type = alloc_type_t::DL_BC;
      tti_sched->bc_allocs.emplace_back(bc_alloc);
    }
  }
}

/*******************************************************
 *                 RAR scheduling
 *******************************************************/

ra_sched::ra_sched(std::map<uint16_t, sched_ue>& ue_db_) :
        log_h(srslte::logmap::get("MAC")),
        ue_db(&ue_db_)
{}

// Schedules RAR
// On every call to this function, we schedule the oldest RAR which is still within the window. If outside the window we
// discard it.
alloc_outcome_t ra_sched::dl_sched(sf_sched *tti_sched)
{
  uint32_t tti_tx_dl = tti_sched->get_tti_tx_dl();
  uint32_t current_sf_idx = tti_sched->get_tti_params().sf_idx_tx_dl;
  uint32_t current_sfn = tti_sched->get_tti_params().sfn_tx_dl;
  rar_aggr_level = 2;

  while (not pending_rars.empty()) {
    // Check if conflict with PBCH & NRS
    //    if (current_sf_idx == 0 || current_sf_idx == 5 || (current_sf_idx == 9 && current_sfn % 2 == 0)) {
    //      printf("RAR SCHED: Current TTI %d conflicts with PBCH & NRS!\n", tti_tx_dl);
    //      return alloc_outcome_t::RB_COLLISION;
    //    }
    if (!srslte_ra_nbiot_is_valid_dl_sf(tti_tx_dl)) {
      log_h->warning("MAC RAR ra_sched::dl_sched TTI %d conflicts with PBCH & NRS!\n", tti_tx_dl);
      printf("MAC RAR ra_sched::dl_sched TTI %d conflicts with PBCH & NRS!\n", tti_tx_dl);
      return alloc_outcome_t::RB_COLLISION;
    }

    if (tti_sched->dl_sched_table[tti_tx_dl]) {
      log_h->warning("MAC RAR ra_sched::dl_sched TTI %d is already scheduled.\n", tti_tx_dl);
      printf("MAC RAR ra_sched::dl_sched TTI %d is already scheduled.\n", tti_tx_dl);
      return alloc_outcome_t::RB_COLLISION;
    }

    // Check the UE search space
    if (tti_tx_dl % 16 >= 8){
      log_h->warning("MAC RAR ra_sched::dl_sched TTI %d is not in the UE search space.\n", tti_tx_dl);
      printf("MAC RAR ra_sched::dl_sched TTI %d is not in the UE search space.\n", tti_tx_dl);
      return alloc_outcome_t::RB_COLLISION;
    }

    sf_sched::pending_rar_t &rar = pending_rars.front();
    uint32_t prach_tti = rar.nprach_tti;

    // Try to schedule DCI + RBGs for RAR Grant
    std::pair<alloc_outcome_t, uint32_t> ret = tti_sched->alloc_rar(rar_aggr_level, rar);

    if (ret.first != alloc_outcome_t::SUCCESS) {
      // try to scheduler next RAR with different RA-RNTI
      continue;
    }

    uint32_t nof_rar_allocs = ret.second;
    if (nof_rar_allocs == rar.nof_grants) {
      // all RAR grants were allocated. Remove pending RAR
      pending_rars.pop_front();

      //Update the sched table
      tti_sched->dl_sched_table[tti_tx_dl] = true; // Set the current TTI occupied
      //printf("DL_sched_table %d: allocate to rar dci\n", tti_tx_dl);
      uint32_t rar_tx_tti = (tti_tx_dl + 5) % 10240;
      while (!srslte_ra_nbiot_is_valid_dl_sf(rar_tx_tti)) {
        rar_tx_tti = (rar_tx_tti + 1) % 10240;
      }
      tti_sched->dl_sched_table[rar_tx_tti] = true;
      //printf("DL_sched_table %d: allocate to rar data\n", rar_tx_tti);
      return alloc_outcome_t::SUCCESS;
    } else {
      // keep the RAR grants that were not scheduled, so we can schedule in next TTI
      printf("RAR SCHED: nof_rar_allocs != rar.nof_grants, keep the RAR grants that were not scheduled\n");
      std::copy(&rar.msg3_grant[nof_rar_allocs], &rar.msg3_grant[rar.nof_grants], &rar.msg3_grant[0]);
      rar.nof_grants -= nof_rar_allocs;
      return alloc_outcome_t::DCI_COLLISION;
    }
  }

  return alloc_outcome_t::DCI_COLLISION;
}

int ra_sched::dl_rach_info(dl_sched_rar_info_t rar_info)
{
  log_h->info("SCHED: New NPRACH tti=%d, preamble=%d, temp_crnti=0x%x, ta_cmd=%d, msg3_size=%d\n",
              rar_info.nprach_tti,
              rar_info.preamble_idx,
              rar_info.temp_crnti,
              rar_info.ta_cmd,
              rar_info.msg3_size);
  // 36.321 Section 5.1.4
  // RA-RNTI = 1 + floor(SFN_id/4) + 256*carrier_id
  // SFN_id = index of the first radio frame of the specified PRACH
  // carrier_id = index of the UL carrier associated with the specified PRACH.
  //              The carrier_id of the anchor carrier is 0. (So 0 for our current cases)
  uint16_t ra_rnti = 1 + (uint16_t) (rar_info.nprach_tti / 10 / 4);

  // find pending rar with same RA-RNTI
  for (sf_sched::pending_rar_t &r : pending_rars) {
    if (r.nprach_tti == rar_info.nprach_tti and ra_rnti == r.ra_rnti) {
      r.msg3_grant[r.nof_grants] = rar_info;
      r.nof_grants++;
      return SRSLTE_SUCCESS;
    }
  }

  // create new RAR
  sf_sched::pending_rar_t p;
  p.ra_rnti = ra_rnti;
  p.nprach_tti = rar_info.nprach_tti;
  p.nof_grants = 1;
  p.msg3_grant[0] = rar_info;
  pending_rars.push_back(p);

  return SRSLTE_SUCCESS;
}

//! Schedule Msg3 grants in UL based on allocated RARs
void ra_sched::ul_sched(sf_sched* sf_dl_sched, sf_sched* sf_msg3_sched)
{
  const std::vector<sf_sched::rar_alloc_t>& alloc_rars = sf_dl_sched->get_allocated_rars();

  for (const auto& rar : alloc_rars) {
    for (uint32_t j = 0; j < rar.rar_grant.nof_grants; ++j) {
      const auto& msg3grant = rar.rar_grant.msg3_grant[j];

      uint16_t crnti   = msg3grant.data.temp_crnti;
      auto     user_it = ue_db->find(crnti);
      if (user_it != ue_db->end() and sf_msg3_sched->alloc_msg3(&user_it->second, msg3grant)) {
        log_h->debug("SCHED: Queueing Msg3 for rnti=0x%x at tti=%d\n", crnti, sf_msg3_sched->get_tti_tx_ul());
        printf("SCHED: Queueing Msg3 for rnti=0x%x at tti=%d\n", crnti, sf_msg3_sched->get_tti_tx_ul());
      } else {
        log_h->error("SCHED: Failed to allocate Msg3 for rnti=0x%x at tti=%d\n", crnti, sf_msg3_sched->get_tti_tx_ul());
      }
    }
  }
}

void ra_sched::reset()
{
  pending_rars.clear();
}

/*******************************************************
*                 Carrier scheduling
*******************************************************/

sched::carrier_sched::carrier_sched(std::map<uint16_t, sched_ue>* ue_db_) :
        ue_db(ue_db_),
        log_h(srslte::logmap::get("MAC ")),
        dl_sched_table(),
        ul_sched_table()
{
  sf_dl_mask.resize(1, 0);
}

sched::carrier_sched::~carrier_sched()
{}

void sched::carrier_sched::reset()
{
  ra_sched_ptr.reset();
  bc_sched_ptr.reset();
}

void sched::carrier_sched::carrier_cfg()
{
  bc_sched_ptr.reset(new bc_sched{});
  ra_sched_ptr.reset(new ra_sched{*ue_db});
}

const sf_sched_result& sched::carrier_sched::generate_tti_result(uint32_t hfn, uint32_t tti_rx, bool dl_flag)
{
  sf_sched_result* sf_result = get_next_sf_result(tti_rx);

  // if it is the first time tti is run, reset vars
  if (tti_rx != sf_result->tti_params.tti_rx) {
    sf_sched* tti_sched = get_sf_sched(tti_rx, dl_sched_table, ul_sched_table);
    *sf_result          = {};

    if(dl_flag){
      // Clear the sched table for the current TTI
      dl_sched_table[tti_rx % 10240] = false;
      /* Schedule DL control data */
      /* Schedule Broadcast data (SIB and paging) */
      bc_sched_ptr->dl_sched(hfn, tti_sched);

      /* Schedule RAR */
      auto ret = ra_sched_ptr->dl_sched(tti_sched);

      /* Schedule Msg3 */
      if(ret==alloc_outcome_t::SUCCESS){
        sf_sched *sf_msg3_sched = get_sf_sched(tti_rx+9, dl_sched_table, ul_sched_table); // NB-IoT MSG3_DELAY_MS = 9
        ra_sched_ptr->ul_sched(tti_sched, sf_msg3_sched);
      }

      /* Schedule DL user data */
      alloc_dl_users(tti_sched);
    } else {
      ul_sched_table[tti_rx % 10240] = false;
      /* Schedule UL user data */
      alloc_ul_users(tti_sched);
    }

    /* Select the winner DCI allocation combination, store all the scheduling results */
    tti_sched->generate_sched_results(sf_result);
  }

  return *sf_result;
}

void sched::carrier_sched::alloc_dl_users(sf_sched* tti_result)
{
  if (sf_dl_mask[tti_result->get_tti_tx_dl() % sf_dl_mask.size()] != 0) {
    return;
  }

  // NOTE: In case of 6 PRBs, do not transmit if there is going to be a PRACH in the UL to avoid collisions
  //  if (cc_cfg->nof_prb() == 6) {
  //    uint32_t tti_rx_ack = tti_result->get_tti_params().tti_rx_ack_dl();
  //    if (srslte_prach_tti_opportunity_config_fdd(cc_cfg->cfg.prach_config, tti_rx_ack, -1)) {
  //      tti_result->reserve_dl_rbgs(0, cc_cfg->nof_rbgs);
  //    }
  //  }

  // call DL scheduler metric to fill RB grid
  sched_users_dl(tti_result);
}

void sched::carrier_sched::sched_users_dl(sf_sched* tti_sched)
{
  if (ue_db->empty()) {
    return;
  }

  // give priority in a time-domain RR basis.
  uint32_t priority_idx = tti_sched->get_tti_tx_dl() % (uint32_t)ue_db->size();
  auto     iter         = ue_db->begin();
  std::advance(iter, priority_idx);
  for (uint32_t ue_count = 0; ue_count < ue_db->size(); ++iter, ++ue_count) {
    if (iter == ue_db->end()) {
      iter = ue_db->begin(); // wrap around
    }
    sched_ue* user = &iter->second;
    allocate_user(user, tti_sched);
  }
}

// HACK: Remember last DL alloc and separate consecutive ones for some time
// TODO: Use HARQ procedure to properly handle
static int last_alloc = 0;
static bool alloc_active = false;

dl_harq_proc* sched::carrier_sched::allocate_user(sched_ue* user, dl_sf_sched_itf* tti_sched)
{
  // Do not allocate a user multiple times in the same tti
  if (tti_sched->is_dl_alloc(user)) {
    return nullptr;
  }
  // Do not allocate a user to an inactive carrier
  //  auto p = user->get_cell_index(cc_cfg->enb_cc_idx);
  //  if (not p.first) {
  //    return nullptr;
  //  }
  //  uint32_t cell_idx = p.second;
  uint32_t cell_idx = 0;

  alloc_outcome_t code;
  uint32_t        tti_dl = tti_sched->get_tti_tx_dl();
  //  dl_harq_proc*   h      = user->get_pending_dl_harq(tti_dl, cell_idx);

  if (alloc_active) {
    if ((tti_dl + 10240 - last_alloc) % 10240 > 25) {
      alloc_active = false;
    }
    return nullptr;
  }

  std::pair<uint32_t, uint32_t> req_bytes = user->get_requested_dl_bytes(cell_idx);

  if (req_bytes.first > 0 /* and req_bytes.second > 5 */) {
    printf("MAC: NB-IoT ===============================Get req_bytes min data %d, max data %d\n", req_bytes.first, req_bytes.second);
    //    code = tti_sched->alloc_dl_user(user, newtx_mask, h->get_id());
    code = tti_sched->alloc_dl_user(user, req_bytes.second);
    if (code == alloc_outcome_t::SUCCESS) {
      last_alloc = tti_dl;
      alloc_active = true;
      return nullptr;
    } else if (code == alloc_outcome_t::DCI_COLLISION) {
      log_h->warning("SCHED: Couldn't find space in PDCCH for DL tx for rnti=0x%x\n", user->get_rnti());
    }
  }
  return nullptr;
}


int sched::carrier_sched::alloc_ul_users(sf_sched* tti_sched)
{
  uint32_t tti_tx_ul = tti_sched->get_tti_tx_ul();

  //  /* reserve PRBs for PRACH */
  //  if (srslte_prach_tti_opportunity_config_fdd(cc_cfg->cfg.prach_config, tti_tx_ul, -1)) {
  //    tti_sched->reserve_ul_prbs(prach_mask, false);
  //    log_h->debug("SCHED: Allocated PRACH RBs. Mask: 0x%s\n", prach_mask.to_hex().c_str());
  //  }
  //
  //  /* reserve PRBs for PUCCH */
  //  tti_sched->reserve_ul_prbs(pucch_mask, cc_cfg->nof_prb() != 6);

  /* Call scheduler for UL data */
  sched_users_ul(tti_sched);

  return SRSLTE_SUCCESS;
}
void sched::carrier_sched::sched_users_ul(sf_sched* tti_sched)
{
  if (ue_db->empty()) {
    return;
  }

  // give priority in a time-domain RR basis
  uint32_t priority_idx = tti_sched->get_tti_tx_ul() % (uint32_t)ue_db->size();

  auto iter = ue_db->begin();
  std::advance(iter, priority_idx);
  //  // allocate reTxs first
  //  for (uint32_t ue_count = 0; ue_count < ue_db->size(); ++iter, ++ue_count) {
  //    if (iter == ue_db->end()) {
  //      iter = ue_db->begin(); // wrap around
  //    }
  //    sched_ue* user = &iter->second;
  //    allocate_user_retx_prbs(user);
  //  }

  // give priority in a time-domain RR basis
  iter = ue_db->begin();
  std::advance(iter, priority_idx);
  for (uint32_t ue_count = 0; ue_count < ue_db->size(); ++iter, ++ue_count) {
    if (iter == ue_db->end()) {
      iter = ue_db->begin(); // wrap around
    }
    sched_ue* user = &iter->second;
    allocate_user_newtx_prbs(user, tti_sched);
  }
}

ul_harq_proc* sched::carrier_sched::allocate_user_newtx_prbs(sched_ue* user, ul_sf_sched_itf* tti_sched)
{
  if (tti_sched->is_ul_alloc(user)) {
    return nullptr;
  }
  uint32_t current_tti = tti_sched->get_tti_tx_ul();
//  auto p = user->get_cell_index(cc_cfg->enb_cc_idx);
//  if (not p.first) {
//    // this cc is not activated for this user
//    return nullptr;
//  }
//  uint32_t cell_idx = p.second;
  uint32_t cell_idx = 0;

  uint32_t      pending_data = user->get_pending_ul_new_data(current_tti);
//  ul_harq_proc* h            = user->get_ul_harq(current_tti, cell_idx);

  // find an empty PID
  if (pending_data > 0) {
    ul_harq_proc::ul_alloc_t alloc{};

    alloc_outcome_t ret = tti_sched->alloc_ul_user(user, alloc);
    if (ret == alloc_outcome_t::SUCCESS) {
      user->ul_buffer_state(0,0, true); // Reset the bsr value to 0
      user->ul_buffer_state(3,0, true); // Reset the bsr value to 0
      return nullptr;
    }
    if (ret == alloc_outcome_t::DCI_COLLISION) {
      log_h->warning("SCHED: Couldn't find space in NPDCCH for UL tx of rnti=0x%x\n", user->get_rnti());
    }
  }
  return nullptr;
}
sf_sched *sched::carrier_sched::get_sf_sched(uint32_t tti_rx, bool* dl_sched_table, bool* ul_sched_table)
{
  sf_sched *ret = &sf_scheds[tti_rx % sf_scheds.size()];
  ret->dl_sched_table = dl_sched_table;
  ret->ul_sched_table = ul_sched_table;
  if (ret->get_tti_rx() != tti_rx) {
    // start new TTI. Bind the struct where the result is going to be stored
    ret->new_tti(tti_rx);
  }
  return ret;
}

sf_sched_result *sched::carrier_sched::get_next_sf_result(uint32_t tti_rx)
{
  return &sf_sched_results[tti_rx % sf_sched_results.size()];
}


int sched::carrier_sched::dl_rach_info(dl_sched_rar_info_t rar_info)
{
  return ra_sched_ptr->dl_rach_info(rar_info);
//return 0;
}

} // namespace sonica_enb
