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

#include "sonica_enb/hdr/stack/mac/scheduler_grid.h"
#include "sonica_enb/hdr/stack/mac/scheduler_carrier.h"
#include "sonica_enb/hdr/stack/mac/scheduler.h"
#include "srslte/common/log_helper.h"
#include "srslte/common/logmap.h"
#include <srslte/interfaces/sched_interface.h>

namespace sonica_enb {

const char *alloc_outcome_t::to_string() const
{
  switch (result) {
    case SUCCESS:
      return "success";
    case DCI_COLLISION:
      return "dci_collision";
    case RB_COLLISION:
      return "rb_collision";
    case ERROR:
      return "error";
    case NOF_RB_INVALID:
      return "invalid nof prbs";
  }
  return "unknown error";
}

tti_params_t::tti_params_t(uint32_t tti_rx_) :
        tti_rx(tti_rx_),
        sf_idx_tx_dl(TTI_ADD(tti_rx, FDD_HARQ_DELAY_UL_MS) % 10),
        tti_tx_dl(TTI_ADD(tti_rx, FDD_HARQ_DELAY_UL_MS)),
        tti_tx_ul(TTI_ADD(tti_rx, (FDD_DELAY_UL_NB_MS + FDD_HARQ_DELAY_DL_MS))),
        sfn_tx_dl(TTI_ADD(tti_rx, FDD_HARQ_DELAY_UL_MS) / 10)
{}


/*******************************************************
 *          TTI resource Scheduling Methods
 *******************************************************/

sf_sched::sf_sched() : log_h(srslte::logmap::get("MAC "))
{}

//void sf_sched::init(const sched_cell_params_t& cell_params_)
//{
//  cc_cfg = &cell_params_;
//  tti_alloc.init(*cc_cfg);
//  max_msg3_prb = std::max(6u, cc_cfg->cfg.cell.nof_prb - (uint32_t)cc_cfg->cfg.nrb_pucch);
//}

void sf_sched::new_tti(uint32_t tti_rx_)
{
  // reset internal state
  bc_allocs.clear();
  rar_allocs.clear();
  data_allocs.clear();
  ul_data_allocs.clear();

  tti_params = tti_params_t{tti_rx_};
//  tti_alloc.new_tti(tti_params);

  // setup first prb to be used for msg3 alloc. Account for potential PRACH alloc
//  last_msg3_prb           = cc_cfg->cfg.nrb_pucch;
//  uint32_t tti_msg3_alloc = TTI_ADD(tti_params.tti_tx_ul, MSG3_DELAY_MS);
//  if (srslte_prach_tti_opportunity_config_fdd(cc_cfg->cfg.prach_config, tti_msg3_alloc, -1)) {
//    last_msg3_prb = std::max(last_msg3_prb, cc_cfg->cfg.prach_freq_offset + 6);
//  }
}

bool sf_sched::is_dl_alloc(sched_ue* user) const
{
  for (const auto& a : data_allocs) {
    if (a.user_ptr == user) {
      return true;
    }
  }
  return false;
}

bool sf_sched::is_ul_alloc(sched_ue* user) const
{
  for (const auto& a : ul_data_allocs) {
    if (a.user_ptr == user) {
      return true;
    }
  }
  return false;
}

sf_sched::ctrl_code_t sf_sched::alloc_dl_ctrl(uint32_t aggr_lvl, uint32_t tbs_bytes, uint16_t rnti)
{
  ctrl_alloc_t ctrl_alloc{};

  // based on rnti, check which type of alloc
  alloc_type_t alloc_type = alloc_type_t::DL_RAR;
  if (rnti == SRSLTE_SIRNTI) {
    alloc_type = alloc_type_t::DL_BC;
  } else if (rnti == SRSLTE_PRNTI) {
    alloc_type = alloc_type_t::DL_PCCH;
  }

  // Allocation Successful
  //  ctrl_alloc.dci_idx = tti_alloc.get_pdcch_grid().nof_allocs() - 1;
  ctrl_alloc.rnti       = rnti;
  ctrl_alloc.req_bytes  = tbs_bytes;
  ctrl_alloc.alloc_type = alloc_type;

  return {alloc_outcome_t::SUCCESS, ctrl_alloc};
}

std::pair<alloc_outcome_t, uint32_t> sf_sched::alloc_rar(uint32_t aggr_lvl, const pending_rar_t &rar)
{
  const uint32_t msg3_grant_size = 3;
  std::pair<alloc_outcome_t, uint32_t> ret = {alloc_outcome_t::ERROR, 0};

  for (uint32_t nof_grants = rar.nof_grants; nof_grants > 0; nof_grants--) {
    uint32_t buf_rar = 8; // 1+6 bytes per RAR subheader+body and 1 byte for Backoff
    uint32_t total_msg3_size = msg3_grant_size;

    // TODO: check if there is enough space for Msg3, try again with a lower number of grants
    //    if (last_msg3_prb + total_msg3_size > max_msg3_prb) {
    //      ret.first = alloc_outcome_t::RB_COLLISION;
    //      continue;
    //    }

    // allocate RBs and PDCCH
    sf_sched::ctrl_code_t ret2 = alloc_dl_ctrl(aggr_lvl, buf_rar, rar.ra_rnti);
    ret.first = ret2.first.result;
    ret.second = 1;

    // if there was no space for the RAR, try again
    if (ret.first == alloc_outcome_t::RB_COLLISION) {
      continue;
    }
    // if any other error, return
    if (ret.first != alloc_outcome_t::SUCCESS) {
      log_h->warning("SCHED: Could not allocate RAR for L=%d, cause=%s\n", aggr_lvl, ret.first.to_string());
      return ret;
    }

    // RAR allocation successful
    sched_interface::dl_sched_rar_t rar_grant = {};
    rar_grant.nof_grants = 1;

    rar_grant.msg3_grant[0].data = rar.msg3_grant[0];
    rar_grant.msg3_grant[0].grant.sc_spacing = 1;
    rar_grant.msg3_grant[0].grant.i_sc = 18;
    rar_grant.msg3_grant[0].grant.i_delay = 0;
    rar_grant.msg3_grant[0].grant.n_rep = 0;
    rar_grant.msg3_grant[0].grant.i_mcs = 0;

//    last_msg3_prb += msg3_grant_size;
    rar_allocs.emplace_back(ret2.second, rar_grant);

    break;
  }
  if (ret.first != alloc_outcome_t::SUCCESS) {
    log_h->warning("SCHED: Failed to allocate RAR due to lack of RBs\n");
  }
  return ret;
}

void sf_sched::set_bc_sched_result(sched_interface::dl_sched_res_t *dl_result)
{
  for (const auto &bc_alloc : bc_allocs) {
	sched_interface::dl_sched_bc_t *bc = &dl_result->bc[dl_result->nof_bc_elems];

        bc->dci = bc_alloc.dci;
        bc->is_new_sib = bc_alloc.is_new_sib;

	dl_result->nof_bc_elems++;
  }
}

void sf_sched::set_rar_sched_result(sched_interface::dl_sched_res_t *dl_result)
{
  for (const auto &rar_alloc : rar_allocs) {
    sched_interface::dl_sched_rar_t *rar = &dl_result->rar[dl_result->nof_rar_elems];

    // Assign NCCE/L
//    rar->dci.location = dci_result[rar_alloc.alloc_data.dci_idx]->dci_pos;

    // NB-IoT specific fields
    rar->dci.mcs_idx = 4; // i_tbs_val
    rar->dci.format = 1; // FormatN1 DCI
    rar->dci.alloc.has_sib1 = false;
    rar->dci.alloc.is_ra = false;
    rar->dci.alloc.i_delay = 0;
    rar->dci.alloc.i_sf = 0; // i_sf_val
    rar->dci.alloc.i_rep = 0; // i_rep_val
    rar->dci.alloc.harq_ack = 1;
    rar->dci.alloc.i_n_start = 0;
    rar->dci.alloc.rnti = rar_alloc.alloc_data.rnti;

    // Setup RAR process
    rar->tbs = rar_alloc.alloc_data.req_bytes;
    rar->nof_grants = rar_alloc.rar_grant.nof_grants;
    std::copy(&rar_alloc.rar_grant.msg3_grant[0], &rar_alloc.rar_grant.msg3_grant[rar->nof_grants], rar->msg3_grant);

    // Print RAR allocation result
    for (uint32_t i = 0; i < rar->nof_grants; ++i) {
      const auto &msg3_grant = rar->msg3_grant[i];
      uint16_t expected_rnti = msg3_grant.data.temp_crnti;
      log_h->info("SCHED: RAR, temp_crnti=0x%x, ra-rnti=%d, rbgs=(%d,%d) \n",
                  expected_rnti,
                  rar_alloc.alloc_data.rnti,
                  rar_alloc.alloc_data.rbg_range.rbg_min,
                  rar_alloc.alloc_data.rbg_range.rbg_max);
    }

    dl_result->nof_rar_elems++;
  }
}

void sf_sched::set_dl_data_sched_result(sched_interface::dl_sched_res_t*    dl_result)
{
  for (const auto& data_alloc : data_allocs) {
    sched_interface::dl_sched_data_t* data = &dl_result->data[dl_result->nof_data_elems];

    // Generate DCI Format1/2/2A
    sched_ue*           user        = data_alloc.user_ptr;
//    uint32_t            cell_index  = user->get_cell_index(cc_cfg->enb_cc_idx).second;
    uint32_t            data_before = user->get_pending_dl_new_data();
//    const dl_harq_proc& dl_harq     = user->get_dl_harq(data_alloc.pid, cell_index);
//    bool                is_newtx    = dl_harq.is_empty();
    bool                is_newtx    = true;

    int tbs = user->generate_formatN1(data, get_tti_tx_dl(), false,data_alloc.i_sf);

    if (tbs <= 0) {
      log_h->warning("SCHED: DL %s failed rnti=0x%x, pid=%d, i_sf=%d, tbs=%d, buffer=%d\n",
                     is_newtx ? "tx" : "retx",
                     user->get_rnti(),
                     data_alloc.pid,
                     data_alloc.i_sf,
                     tbs,
                     user->get_pending_dl_new_data());
      continue;
    }

    // Print Resulting DL Allocation
    log_h->info("SCHED: DL %s rnti=0x%x, cc=%d, pid=%d, i_sf=%d, tbs=%d, buffer=%d/%d\n",
                !is_newtx ? "retx" : "tx",
                user->get_rnti(),
                0, // CC index
                data_alloc.pid,
                data_alloc.i_sf,
                tbs,
                data_before,
                user->get_pending_dl_new_data());

    dl_result->nof_data_elems++;
  }
                                        }

void sf_sched::set_ul_sched_result(sched_interface::ul_sched_res_t*    ul_result)
{
  /* Set UL data DCI locs and format */
  for (const auto& ul_alloc : ul_data_allocs) {
    sched_interface::ul_sched_data_t* npusch = &ul_result->npusch[ul_result->nof_dci_elems];

    sched_ue* user = ul_alloc.user_ptr;

    /* Set fixed mcs from ul allcoation */
    int fixed_mcs = ul_alloc.mcs;

    /* Generate DCI FormatN0 */
    //    uint32_t pending_data_before = user->get_pending_ul_new_data(get_tti_tx_ul());
    int tbs = user->generate_formatN0(npusch, ul_alloc.alloc, ul_alloc.needs_npdcch(), fixed_mcs);

    //    ul_harq_proc* h = user->get_ul_harq(get_tti_tx_ul(), cell_index);

    if (tbs <= 0) {
      log_h->warning("SCHED: Error %s %s rnti=0x%x \n",
                     ul_alloc.type == ul_alloc_t::MSG3 ? "Msg3" : "UL",
                     ul_alloc.is_retx() ? "retx" : "tx",
                     user->get_rnti());
      //                     h->get_id(),
      //                     user->get_pending_ul_new_data(get_tti_tx_ul()));
      continue;
    }

    // Print Resulting UL Allocation
    log_h->info("SCHED: %s %s rnti=0x%x,tbs=%d\n",
                ul_alloc.is_msg3() ? "Msg3" : "UL",
                ul_alloc.is_retx() ? "retx" : "tx",
                user->get_rnti(),
                tbs);
    //                h->get_id(),
    //                h->nof_retx(0),
    //                user->get_pending_ul_new_data(get_tti_tx_ul()),
    //                pending_data_before,
    //                user->get_pending_ul_old_data(cell_index));

    ul_result->nof_dci_elems++;
  }
}

/// I_sf to N_sf from Table 16.4.1.3-1 in TS 36.213 R15
const int i_sf_to_n_sf_npdsch[8] = {1, 2, 3, 4, 5, 6, 8, 10};

alloc_outcome_t sf_sched::alloc_dl_user(sched_ue* user, uint32_t max_data)
{
  if (is_dl_alloc(user)) {
    log_h->warning("SCHED: Attempt to assign multiple harq pids to the same user rnti=0x%x\n", user->get_rnti());
    return alloc_outcome_t::ERROR;
  }

  // Check if allocation would cause segmentation
  //  uint32_t            ue_cc_idx = user->get_cell_index(cc_cfg->enb_cc_idx).second;
  //  const dl_harq_proc& h         = user->get_dl_harq(pid, ue_cc_idx);
  //  if (h.is_empty()) {
  //    // It is newTx
  //    rbg_range_t r = user->get_required_dl_rbgs(ue_cc_idx);
  //    if (r.rbg_min > user_mask.count()) {
  //      log_h->warning("The number of RBGs allocated to rnti=0x%x will force segmentation\n", user->get_rnti());
  //      return alloc_outcome_t::NOF_RB_INVALID;
  //    }
  //  }


  // Try to allocate RBGs and DCI
  uint32_t tti_tx_dl = get_tti_tx_dl();

  if (!srslte_ra_nbiot_is_valid_dl_sf(tti_tx_dl)) {
    log_h->warning("SCHED: TTI %d is not valid for DL data.\n", tti_tx_dl);
    printf("SCHED: TTI %d is not valid for DL data.\n", tti_tx_dl);
    return alloc_outcome_t::RB_COLLISION;
  }

  if (dl_sched_table[tti_tx_dl]){
    log_h->warning("SCHED: TTI %d is already scheduled.\n", tti_tx_dl);
    printf("SCHED: TTI %d is already scheduled.\n", tti_tx_dl);
    return alloc_outcome_t::RB_COLLISION;
  }

  // Check the UE search space
  // TODO: Change the search space for different stages
  if (tti_tx_dl % 8 >= 2){
    log_h->warning("MAC DL Data sf_sched::alloc_dl_user TTI %d is not in the UE search space.\n", tti_tx_dl);
    printf("MAC DL data sf_sched::alloc_dl_user TTI %d is not in the UE search space.\n", tti_tx_dl);
    return alloc_outcome_t::RB_COLLISION;
  }


  // Checking if it is a SIB1 TTI
  bc_sched bc_sched_test;
  if (bc_sched_test.is_sib1_sf(tti_tx_dl / 10, tti_tx_dl % 10)) {
    log_h->warning("MAC DL Data sf_sched::alloc_dl_user TTI %d is occupied by SIB1.\n", tti_tx_dl);
    printf("MAC DL Data sf_sched::alloc_dl_user TTI %d is occupied by SIB1.\n", tti_tx_dl);
    return alloc_outcome_t::RB_COLLISION;
  }

  // Update dl_sched table. Currently Allocate 1 sf once to a user. Future: Change the allocation scheme
  uint32_t alloc_i = 0, i_sf = 0;

  // TODO: NB-IoT: if max_data > 20 bytes, allocate 3 TTI slots
  if(max_data > 20){
    i_sf = 2;
  }

  uint32_t alloc_sf_num = i_sf_to_n_sf_npdsch[i_sf];

  // Set the DCI TTI in the sched_table
  dl_sched_table[tti_tx_dl] = true;
  printf("SCHED: TTI %d sf_sched::alloc_dl_user DCI.\n", tti_tx_dl);
  uint32_t data_tx_tti = (tti_tx_dl + 5) % 10240;

  while (alloc_sf_num > 0) {
    uint32_t alloc_tti = (data_tx_tti + alloc_i) % 10240;
    if (srslte_ra_nbiot_is_valid_dl_sf(alloc_tti) && !dl_sched_table[alloc_tti] &&
        !bc_sched_test.is_sib1_sf(alloc_tti / 10, alloc_tti % 10)) {
      printf("DL_sched_table %d: allocate to user 0x%x npdsch\n", alloc_tti, user->get_rnti());
      dl_sched_table[alloc_tti] = true;
      alloc_sf_num--;
    } else {
      printf("DL_sched_table Jump %d: user 0x%x npdsch collision with existing alloc\n", alloc_tti, user->get_rnti());
    }
    alloc_i++;
  }

  // Allocation Successful
  dl_alloc_t alloc;
  alloc.user_ptr = user;
  alloc.i_sf     = i_sf;
  //  alloc.pid       = pid;
  data_allocs.push_back(alloc);

  Debug("MAC:NB-IoT: ----------------=================---------------Allocate an UL grant here!\n");
  printf("MAC:NB-IoT: -------------===============------------------Allocate an UL grant here!\n");
  user->ul_buffer_state(3, 70, false);
  user->ul_wait_timer(30);

  return alloc_outcome_t::SUCCESS;
}

alloc_outcome_t sf_sched::alloc_ul(sched_ue*                    user,
                                   ul_harq_proc::ul_alloc_t     alloc,
                                   sf_sched::ul_alloc_t::type_t alloc_type,
                                   uint32_t                     mcs)
{
  // Check whether user was already allocated
  if (is_ul_alloc(user)) {
    log_h->warning("SCHED: Attempt to assign multiple ul_harq_proc to the same user rnti=0x%x\n", user->get_rnti());
    return alloc_outcome_t::ERROR;
  }

  // Allocate RBGs and DCI space
  bool            needs_npdcch = alloc_type == ul_alloc_t::ADAPT_RETX or alloc_type == ul_alloc_t::NEWTX;
  uint32_t tti_tx_dl = get_tti_tx_dl();

  if(alloc_type == ul_alloc_t::MSG3){
    uint32_t rar_dci_tx_tti = (tti_tx_dl + 10240 - 9) % 10240; // NB-IoT MSG3_DELAY_MS = 9
    uint32_t rar_tx_tti = (rar_dci_tx_tti + 5) % 10240;
    while (!srslte_ra_nbiot_is_valid_dl_sf(rar_tx_tti)) {
      rar_tx_tti = (rar_tx_tti + 1) % 10240;
    }

    uint32_t msg3_rx_tti = rar_tx_tti + 13;

    // Check MSG3 TTIs (4 continuous sub frames)
    for(uint32_t i=0; i<alloc.len; i++){
      if(ul_sched_table[(msg3_rx_tti + i) % 10240]){
        return alloc_outcome_t::RB_COLLISION;
      }
    }
    // Set the MSG3 TTIs as occupied
    for(uint32_t i=0; i<alloc.len; i++) {
      printf("MAC sf_sched::alloc_ul: Set ul_sched_table %d true for MSG3\n", msg3_rx_tti+i);
      ul_sched_table[(msg3_rx_tti + i) % 10240] = true;
    }
  }

  // Generate PDCCH except for RAR and non-adaptive retx
  if (needs_npdcch) {
    // UL Data allocation. Currently use mcs=12
    alloc.sc_num = 12;
    alloc.len = 4;

    // TODO: NB-IoT: Decide the MCS based on the estimated pending data now
    uint32_t pending_data = user->get_pending_ul_new_data(tti_tx_dl);
    if(pending_data >= 125){
      alloc.len = 6;
      mcs = 10;
    } else {
      mcs = 9;
    }

    uint32_t tti_tx_ul = get_tti_tx_ul();

    printf("MAC UL Data sf_sched::alloc_ul tti_tx_ul=%d, tti_tx_dl=%d.\n", tti_tx_ul, tti_tx_dl);
    if (user->msg_wait_timer != 0){
      log_h->warning("MAC UL Data sf_sched::alloc_ul: The wait timer is not finished at TTI %d.\n", tti_tx_dl);
      printf("MAC UL Data sf_sched::alloc_ul: The wait timer is not finished at TTI %d.\n", tti_tx_dl);
      user->msg_wait_timer--;
      return alloc_outcome_t::DCI_COLLISION;
    }

    // Check if the DL grant is in the valid TTI
    if(!srslte_ra_nbiot_is_valid_dl_sf(tti_tx_dl)){
      log_h->warning("MAC UL Data sf_sched::alloc_ul: TTI %d is not the valid TTI for DL grant.\n", tti_tx_dl);
      printf("MAC UL Data sf_sched::alloc_ul: TTI %d is not the valid TTI for DL grant.\n", tti_tx_dl);
      return alloc_outcome_t::DCI_COLLISION;
    } else if(dl_sched_table[tti_tx_dl]){
      log_h->warning("MAC UL Data sf_sched::alloc_ul: TTI %d ahs been allocated. No space for DL grant.\n", tti_tx_dl);
      printf("MAC UL Data sf_sched::alloc_ul: TTI %d ahs been allocated. No space for DL grant.\n", tti_tx_dl);
      return alloc_outcome_t::DCI_COLLISION;
    } else if (tti_tx_dl % 8 >= 2){
      // TODO: Check if the DL grant is in the UE search space
      log_h->warning("MAC UL Data sf_sched::alloc_ul: TTI %d is not in the UE search space.\n", tti_tx_dl);
      printf("MAC UL Data sf_sched::alloc_ul: TTI %d is not in the UE search space.\n", tti_tx_dl);
      return alloc_outcome_t::DCI_COLLISION;
    }

    // Check UL DATA TTIs (4 continuous sub frames)
    for(uint32_t i=0; i<alloc.len; i++){
      if(ul_sched_table[tti_tx_ul % 10240]){
        return alloc_outcome_t::RB_COLLISION;
      }
    }
    // Set the MSG3 TTIs as occupied
    for(uint32_t i=0; i<alloc.len; i++) {
      printf("MAC sf_sched::alloc_ul: ------------------------ Set ul_sched_table %d true for UL NEWTX\n", tti_tx_ul+i);
      ul_sched_table[tti_tx_ul % 10240] = true;
    }

    // Also set the DL grant position in the dl_sched_table
    ul_sched_table[tti_tx_dl] = true;
  }

  ul_alloc_t ul_alloc = {};
  ul_alloc.type       = alloc_type;
  ul_alloc.user_ptr   = user;
  ul_alloc.alloc      = alloc;
  ul_alloc.mcs        = mcs;
  ul_data_allocs.push_back(ul_alloc);

  return alloc_outcome_t::SUCCESS;
}

alloc_outcome_t sf_sched::alloc_msg3(sched_ue* user, const sched_interface::dl_sched_rar_grant_t& rargrant)
{
  // Derive PRBs from allocated RAR grants
  ul_harq_proc::ul_alloc_t msg3_alloc = {};
  if (rargrant.grant.i_mcs == 0){
    // currently only support mcs0
    msg3_alloc.sc_num = 12;
    msg3_alloc.len = 4;
  } else {
    log_h->warning("SCHED: Only support MCS 0 allocation for user rnti=0x%x \n", user->get_rnti());
    return alloc_outcome_t::ERROR;
  }

  alloc_outcome_t ret = alloc_ul(user, msg3_alloc, sf_sched::ul_alloc_t::MSG3, rargrant.grant.i_mcs);
  if (not ret) {
    log_h->warning("SCHED: Could not allocate msg3 within (%d,%d)\n", msg3_alloc.sc_num, msg3_alloc.len);
  }
  return ret;
}

alloc_outcome_t sf_sched::alloc_ul_user(sched_ue* user, ul_harq_proc::ul_alloc_t alloc)
{
  // check whether adaptive/non-adaptive retx/newtx
  sf_sched::ul_alloc_t::type_t alloc_type = ul_alloc_t::NEWTX;
  // TODO: Add HARQ procedure checking
  return alloc_ul(user, alloc, alloc_type);
}

void sf_sched::generate_sched_results(sf_sched_result* sf_result)
{
  /* Pick one of the possible DCI masks */
  //  pdcch_grid_t::alloc_result_t dci_result;

  /* Generate DCI formats and fill sched_result structs */
  set_bc_sched_result(&sf_result->dl_sched_result);

  set_rar_sched_result(&sf_result->dl_sched_result);

  set_dl_data_sched_result(&sf_result->dl_sched_result);

  //  set_dl_data_sched_result(dci_result, &sf_result->dl_sched_result);
  set_ul_sched_result(&sf_result->ul_sched_result);

}

} // namespace sonica_enb
