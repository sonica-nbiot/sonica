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

#include <pthread.h>
#include <srslte/interfaces/sched_interface_nb.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "sonica_enb/hdr/stack/mac/mac.h"
#include "srslte/common/log.h"
#include "srslte/common/log_helper.h"
#include "srslte/common/rwlock_guard.h"
#include "srslte/common/time_prof.h"

//#define WRITE_SIB_PCAP
using namespace asn1::rrc;

namespace sonica_enb {

//:
//last_rnti(0),
//rar_pdu_msg(sched_interface::MAX_RAR_LIST),
//    rar_payload(),
//    common_buffers(SRSLTE_MAX_CARRIERS)
mac::mac() :
  last_rnti(0),
  rar_pdu_msg(sched_interface::MAX_RAR_LIST),
  rar_payload(),
  common_buffers(SRSLTE_MAX_CARRIERS)
{
  pthread_rwlock_init(&rwlock, nullptr);
}

mac::~mac()
{
//  stop();
  pthread_rwlock_destroy(&rwlock);
}

bool mac::init(const mac_args_t &args_,
//               const cell_list_t&       cells_,
//               phy_interface_stack_nb* phy,
               rlc_interface_mac*       rlc,
               rrc_interface_mac*       rrc,
               stack_interface_mac_nb *stack_,
               srslte::log_ref log_h_)
{
  started = false;

  if (/* phy && */ log_h_) {
//    phy_h = phy;
    rlc_h = rlc;
    rrc_h = rrc;
    stack = stack_;
    log_h = log_h_;

    args = args_;
//    cells = cells_;

    stack_task_queue = stack->make_task_queue();

    scheduler.init();

    // Set default scheduler configuration
//    scheduler.set_sched_cfg(&args.sched);

    // Init softbuffer for SI messages
//    common_buffers.resize(cells.size());
    common_buffers.resize(1); // Use 1 as the tmp size

    for (auto &cc : common_buffers) {
      for (int i = 0; i < NOF_BCCH_DLSCH_MSG; i++) {
        srslte_softbuffer_tx_init(&cc.bcch_softbuffer_tx[i], args.nof_prb);
      }
      // Init softbuffer for PCCH
      srslte_softbuffer_tx_init(&cc.pcch_softbuffer_tx, args.nof_prb);

      // Init softbuffer for RAR
      srslte_softbuffer_tx_init(&cc.rar_softbuffer_tx, args.nof_prb);
    }

    reset();

    started = true;
  }

  return started;
}

void mac::stop()
{
  srslte::rwlock_write_guard lock(rwlock);
  if (started) {
    ue_db.clear();
    for (auto& cc : common_buffers) {
      for (int i = 0; i < NOF_BCCH_DLSCH_MSG; i++) {
        srslte_softbuffer_tx_free(&cc.bcch_softbuffer_tx[i]);
      }
      srslte_softbuffer_tx_free(&cc.pcch_softbuffer_tx);
      srslte_softbuffer_tx_free(&cc.rar_softbuffer_tx);
      started = false;
    }
  }
}

// Implement Section 5.9
void mac::reset()
{
  Info("Resetting MAC\n");

  last_rnti = 4097;

  /* Setup scheduler */
  scheduler.reset();
}

uint16_t mac::allocate_rnti()
{
  std::lock_guard<std::mutex> lock(rnti_mutex);

  // Assign a c-rnti
  uint16_t rnti = last_rnti++;
  if (last_rnti >= 60000) {
    last_rnti = 4097;
  }

  return rnti;
}

/********************************************************
 *
 * RLC interface
 *
 *******************************************************/

int mac::rlc_buffer_state(uint16_t rnti, uint32_t lc_id, uint32_t tx_queue, uint32_t retx_queue)
{
  srslte::rwlock_read_guard lock(rwlock);
  int                       ret = -1;
  if (ue_db.count(rnti)) {
    if (rnti != SRSLTE_MRNTI) {
      ret = scheduler.dl_rlc_buffer_state(rnti, lc_id, tx_queue, retx_queue);
    } else {
      for (uint32_t i = 0; i < mch.num_mtch_sched; i++) {
        if (lc_id == mch.mtch_sched[i].lcid) {
          mch.mtch_sched[i].lcid_buffer_size = tx_queue;
        }
      }
      ret = 0;
    }
  } else {
    Error("User rnti=0x%x not found\n", rnti);
  }
  return ret;
}

// Update UE configuration
int mac::ue_cfg(uint16_t rnti, sched_interface::ue_cfg_t* cfg)
{
  srslte::rwlock_read_guard lock(rwlock);

  auto it     = ue_db.find(rnti);
  ue*  ue_ptr = nullptr;
  if (it == ue_db.end()) {
    Error("User rnti=0x%x not found\n", rnti);
    return SRSLTE_ERROR;
  }
  ue_ptr = it->second.get();

  // Start TA FSM in UE entity
  //  ue_ptr->start_ta();

  // Add RNTI to the PHY (pregenerate signals) now instead of after PRACH
  if (not ue_ptr->is_phy_added) {
    // Future: Add the PHY add_rnti
    //    Info("Registering RNTI=0x%X to PHY...\n", rnti);
    //    // Register new user in PHY with first CC index
    //    if (phy_h->add_rnti(rnti, (SRSLTE_MRNTI) ? 0 : cfg->supported_cc_list.front().enb_cc_idx, false) ==
    //    SRSLTE_ERROR) {
    //      Error("Registering new UE RNTI=0x%X to PHY\n", rnti);
    //    }
    //    Info("Done registering RNTI=0x%X to PHY...\n", rnti);
    ue_ptr->is_phy_added = true;
  }

  // Update Scheduler configuration
  if (cfg != nullptr and scheduler.ue_cfg(rnti, *cfg) == SRSLTE_ERROR) {
    Error("Registering new UE rnti=0x%x to SCHED\n", rnti);
    return SRSLTE_ERROR;
  }
  return SRSLTE_SUCCESS;
}

// Removes UE from DB
int mac::ue_rem(uint16_t rnti)
{
  int ret = -1;
  {
    srslte::rwlock_read_guard lock(rwlock);
    if (ue_db.count(rnti)) {
      // phy_h->rem_rnti(rnti);
      scheduler.ue_rem(rnti);
      ret = 0;
    } else {
      Error("User rnti=0x%x not found\n", rnti);
    }
  }
  if (ret) {
    return ret;
  }
  srslte::rwlock_write_guard lock(rwlock);
  if (ue_db.count(rnti)) {
    ue_db.erase(rnti);
    Info("User rnti=0x%x removed from MAC/PHY\n", rnti);
  } else {
    Error("User rnti=0x%x already removed\n", rnti);
  }
  return 0;
}

// Called after Msg3
int mac::ue_set_crnti(uint16_t temp_crnti, uint16_t crnti, sched_interface::ue_cfg_t* cfg)
{
  int ret = ue_cfg(crnti, cfg);
  if (ret != SRSLTE_SUCCESS) {
    return ret;
  }
  srslte::rwlock_read_guard lock(rwlock);
  if (temp_crnti == crnti) {
    // if RNTI is maintained, Msg3 contained a RRC Setup Request
    scheduler.dl_mac_buffer_state(crnti, (uint32_t)srslte::dl_sch_lcid::CON_RES_ID);
  } else {
    // C-RNTI corresponds to older user. No Handover for NB-IoT, Error.
    printf("MAC::ue_set_crnti: C-RNTI corresponds to older user.");
    ret = SRSLTE_ERROR;
  }
  return ret;
}

/********************************************************
 *
 * PHY interface
 *
 *******************************************************/

int mac::crc_info(uint32_t tti_rx, uint16_t rnti, uint32_t nof_bytes, bool crc)
{
  int ret = SRSLTE_ERROR;
  log_h->step(tti_rx);
  srslte::rwlock_read_guard lock(rwlock);

  if (ue_db.count(rnti)) {
    ue_db[rnti]->set_tti(tti_rx);
    ue_db[rnti]->metrics_rx(crc, nof_bytes);

    //    std::array<int, SRSLTE_MAX_CARRIERS> enb_ue_cc_map = scheduler.get_enb_ue_cc_map(rnti);
    //    if (enb_ue_cc_map[enb_cc_idx] < 0) {
    //      Error("User rnti=0x%x is not activated for carrier %d\n", rnti, enb_cc_idx);
    //      return ret;
    //    }
    //    uint32_t ue_cc_idx = enb_ue_cc_map[enb_cc_idx];
    //    uint32_t ue_cc_idx = 0;

    // push the pdu through the queue if received correctly
    if (crc) {
      Info("Pushing PDU rnti=0x%x, tti_rx=%d, nof_bytes=%d\n", rnti, tti_rx, nof_bytes);
      ue_db[rnti]->push_pdu(tti_rx, nof_bytes);
      stack_task_queue.push([this]() { process_pdus(); });
    } else {
      ue_db[rnti]->deallocate_pdu(tti_rx);
    }

    // Scheduler uses eNB's CC mapping. Update harq info
    //    ret = scheduler.ul_crc_info(tti_rx, rnti, enb_cc_idx, crc);

  } else {
    Error("User rnti=0x%x not found\n", rnti);
  }

  return ret;
}

bool mac::process_pdus()
{
  srslte::rwlock_read_guard lock(rwlock);
  bool                      ret = false;
  for (auto& u : ue_db) {
    ret |= u.second->process_pdus();
  }
  return ret;
}

void mac::rach_detected(uint32_t tti, uint32_t preamble_idx, uint32_t time_adv)
{
  static srslte::mutexed_tprof<srslte::avg_time_stats> rach_tprof("rach_tprof", "MAC", 1);
  log_h->step(tti);
  auto rach_tprof_meas = rach_tprof.start();

  uint16_t rnti = allocate_rnti();

  // Create new UE
  std::unique_ptr<ue> ue_ptr{new ue(rnti, &scheduler, rlc_h, log_h)};

//  // Set PCAP if available
//  if (pcap != nullptr) {
//    ue_ptr->start_pcap(pcap);
//  }

  {
    srslte::rwlock_write_guard lock(rwlock);
    ue_db[rnti] = std::move(ue_ptr);
  }

  stack_task_queue.push([this, rnti, tti, preamble_idx, time_adv, rach_tprof_meas]() mutable {
//    rach_tprof_meas.defer_stop();
    // Generate RAR data
    sched_interface::dl_sched_rar_info_t rar_info = {};
    rar_info.preamble_idx = preamble_idx;
    rar_info.ta_cmd = time_adv;
    rar_info.temp_crnti = rnti;
    rar_info.msg3_size = 7;
    rar_info.nprach_tti = tti;

    // Add new user to the scheduler
    sched_interface::ue_cfg_t ue_cfg = {};

    // Future: NB UE parameters init here
    ue_cfg.ue_bearers[0].direction             = sonica_enb::sched_interface::ue_bearer_cfg_t::BOTH;
    ue_cfg.dl_cfg.tm                           = SRSLTE_TM1;

    if (scheduler.ue_cfg(rnti, ue_cfg) != SRSLTE_SUCCESS) {
      Error("Registering new user rnti=0x%x to SCHED\n", rnti);
      return;
    }

    // Register new user in RRC
    rrc_h->add_user(rnti, ue_cfg);

//    // Add temporal rnti to the PHY
//    if (phy_h->add_rnti(rnti, enb_cc_idx, true) != SRSLTE_SUCCESS) {
//      Error("Registering temporal-rnti=0x%x to PHY\n", rnti);
//      return;
//    }
//
    // Trigger scheduler RACH
    scheduler.dl_rach_info(rar_info);

    log_h->info("RACH:  tti=%d, preamble=%d, offset=%d, temp_crnti=0x%x\n", tti, preamble_idx, time_adv, rnti);
    log_h->console("RACH:  tti=%d, preamble=%d, offset=%d, temp_crnti=0x%x\n", tti, preamble_idx, time_adv, rnti);
  });
}

int mac::get_dl_sched(uint32_t hfn, uint32_t tti_tx_dl, dl_sched_list_t &dl_sched_res_list)
{
  if (!started) {
    return 0;
  }

  log_h->step(TTI_SUB(tti_tx_dl, FDD_HARQ_DELAY_UL_MS));

  // Run scheduler with current info
  sched_interface::dl_sched_res_t sched_result = {};
  if (scheduler.dl_sched(hfn,tti_tx_dl, sched_result) < 0) {
    Error("Running scheduler\n");
    return SRSLTE_ERROR;
  }

  int n = 0;
  dl_sched_t *dl_sched_res = &dl_sched_res_list[0];

  // Copy User data
  {
    srslte::rwlock_read_guard lock(rwlock);

    // Copy data grants
    for (uint32_t i = 0; i < sched_result.nof_data_elems; i++) {

      // Get UE
      uint16_t rnti = sched_result.data[i].dci.alloc.rnti;

      if (ue_db.count(rnti)) {
        // Copy dci info
        dl_sched_res->npdsch.dci = sched_result.data[i].dci;
        dl_sched_res->npdsch.has_npdcch = true;

        for (uint32_t tb = 0; tb < SRSLTE_MAX_TB; tb++) {
//          dl_sched_res->npdsch.softbuffer_tx[tb] =
//              ue_db[rnti]->get_tx_softbuffer(sched_result.data[i].dci.ue_cc_idx, sched_result.data[i].dci.pid, tb);

          if (sched_result.data[i].nof_pdu_elems[tb] > 0) {
            /* Get PDU if it's a new transmission */
            dl_sched_res->npdsch.data[tb] = ue_db[rnti]->generate_pdu(0, // tmp CC_index
                                                                        0, // tmp pid
                                                                        tb,
                                                                        sched_result.data[i].pdu[tb],
                                                                        sched_result.data[i].nof_pdu_elems[tb],
                                                                        sched_result.data[i].tbs[tb]);

            if (!dl_sched_res->npdsch.data[tb]) {
              Error("Error! PDU was not generated (rnti=0x%04x, tb=%d)\n", rnti, tb);
            }

//            if (pcap) {
//              pcap->write_dl_crnti(dl_sched_res->npdsch.data[tb], sched_result.data[i].tbs[tb], rnti, true, tti_tx_dl, enb_cc_idx);
//            }
          } else {
            /* TB not enabled OR no data to send: set pointers to NULL  */
            dl_sched_res->npdsch.data[tb] = nullptr;
          }
        }
        n++;
      } else {
        Warning("Invalid DL scheduling result. User 0x%x does not exist\n", rnti);
      }
    }

    // No more uses of shared ue_db beyond here
  }

  // Copy RAR grants
  for (uint32_t i = 0; i < sched_result.nof_rar_elems; i++) {
    // Copy dci info
    dl_sched_res->npdsch.dci = sched_result.rar[i].dci;
    dl_sched_res->npdsch.has_npdcch = true;

    // Set softbuffer (there are no retx in RAR but a softbuffer is required)
    //    dl_sched_res->pdsch.softbuffer_tx[0] = &common_buffers[0].rar_softbuffer_tx;

    // Assemble PDU
    dl_sched_res->npdsch.data[0] = assemble_rar(
            sched_result.rar[i].msg3_grant, sched_result.rar[i].nof_grants, i, sched_result.rar[i].tbs, tti_tx_dl);

    n++;
  }

  // Copy SIB grants
  for (uint32_t i = 0; i < sched_result.nof_bc_elems; i++) {
    // Copy dci info
    dl_sched_res->npdsch.dci = sched_result.bc[i].dci;
    dl_sched_res->npdsch.has_npdcch = false;
    dl_sched_res->npdsch.is_new_sib = sched_result.bc[i].is_new_sib;

    // Assemble PDU
    if (dl_sched_res->npdsch.dci.alloc.has_sib1) {
      uint8_t *sib1_content = rrc_h->read_pdu_bcch_dlsch(0);

      // HACK: Patch HFN field in SIB1, we don't go over RRC to do this
      uint8_t s1 = sib1_content[1] & 0xF0;
      s1 |= (hfn >> 6) & 0x0F;
      sib1_content[1] = s1;
      uint8_t s2    = sib1_content[2] & 0x0F;
      s2 |= ((hfn >> 2) & 0x0F) << 4;
      sib1_content[2] = s2;

      dl_sched_res->npdsch.data[0] = sib1_content;
    } else {
      dl_sched_res->npdsch.data[0] = rrc_h->read_pdu_bcch_dlsch(1);
    }

    n++;
  }

  dl_sched_res->nof_grants = n;

  if (n > 1) {
    Error("DL_SCHED: scheduling conflict, %d item generated\n", n);
  }

  // Number of CCH symbols
  //  dl_sched_res->cfi = sched_result.cfi;

  return SRSLTE_SUCCESS;
}

uint8_t *mac::assemble_rar(sched_interface::dl_sched_rar_grant_t *grants,
                           uint32_t nof_grants,
                           int rar_idx,
                           uint32_t pdu_len,
                           uint32_t tti)
{
  uint8_t grant_buffer[64] = {};
  if (pdu_len < rar_payload_len) {
    srslte::rar_pdu *pdu = &rar_pdu_msg[rar_idx];
    rar_payload[rar_idx].clear();
    pdu->init_tx(&rar_payload[rar_idx], pdu_len);
    for (uint32_t i = 0; i < nof_grants; i++) {
      srslte_nb_dci_rar_pack(&grants[i].grant, grant_buffer);
      if (pdu->new_subh()) {
        pdu->get()->set_rapid(grants[i].data.preamble_idx);
        pdu->get()->set_ta_cmd(grants[i].data.ta_cmd);
        pdu->get()->set_temp_crnti(grants[i].data.temp_crnti);
        pdu->get()->set_sched_grant(grant_buffer); // TODO: Change to NB-IoT RAR
      }
    }
    pdu->write_packet(rar_payload[rar_idx].msg);
    return rar_payload[rar_idx].msg;
  } else {
    Error("Assembling RAR: pdu_len > rar_payload_len (%d>%d)\n", pdu_len, rar_payload_len);
    return nullptr;
  }
}

/* Pack RAR UL dci as defined in Section 6.2 of 36.213 */
void mac::srslte_nb_dci_rar_pack(srslte_nbiot_dci_rar_grant_t *rar, uint8_t payload[SRSLTE_RAR_GRANT_LEN])
{
  uint8_t *ptr = payload;
  srslte_bit_unpack(rar->sc_spacing, &ptr, 1);
  srslte_bit_unpack(rar->i_sc, &ptr, 6);
  srslte_bit_unpack(rar->i_delay, &ptr, 2);
  srslte_bit_unpack(rar->n_rep, &ptr, 3);
  srslte_bit_unpack(rar->i_mcs, &ptr, 3);
}

int mac::get_ul_sched(uint32_t hfn, uint32_t tti_tx_ul, ul_sched_list_t& ul_sched_res_list)
{
  if (!started) {
    return SRSLTE_SUCCESS;
  }

  log_h->step(TTI_SUB(tti_tx_ul, FDD_HARQ_DELAY_UL_MS + FDD_HARQ_DELAY_DL_MS));

  // Execute TA FSM (JH: Does NB IoT need it?)
  //  for (auto& ue : ue_db) {
  //    uint32_t nof_ta_count = ue.second->tick_ta_fsm();
  //    if (nof_ta_count) {
  //      scheduler.dl_mac_buffer_state(ue.first, (uint32_t)srslte::dl_sch_lcid::TA_CMD, nof_ta_count);
  //    }
  //  }

  ul_sched_t* phy_ul_sched_res = &ul_sched_res_list[0];

  // Run scheduler with current info
  sched_interface::ul_sched_res_t sched_result = {};
  if (scheduler.ul_sched(hfn, tti_tx_ul, sched_result) < 0) {
    Error("Running scheduler\n");
    return SRSLTE_ERROR;
  }

  {
    srslte::rwlock_read_guard lock(rwlock);

    // Copy DCI grants
    phy_ul_sched_res->nof_grants = 0;
    int n                        = 0;
    for (uint32_t i = 0; i < sched_result.nof_dci_elems; i++) {
      if (sched_result.npusch[i].tbs > 0) {
        // Get UE
        uint16_t rnti = sched_result.npusch[i].dci.rnti;

        if (ue_db.count(rnti)) {
          // Copy grant info
          phy_ul_sched_res->npusch[n].current_tx_nb = sched_result.npusch[i].current_tx_nb;
          phy_ul_sched_res->npusch[n].needs_npdcch  = sched_result.npusch[i].needs_npdcch;
          phy_ul_sched_res->npusch[n].dci           = sched_result.npusch[i].dci;

          /// FUTURE: HARQ Related
          //          phy_ul_sched_res->pusch[n].softbuffer_rx =
          //                          ue_db[rnti]->get_rx_softbuffer(sched_result.pusch[i].dci.ue_cc_idx, tti_tx_ul);
          //          printf("mac::get_ul_sched 5.6, sched_result.npusch[i].tbs = %d\n",sched_result.npusch[i].tbs);
          //          if (sched_result.npusch[n].current_tx_nb == 0) {
          //            srslte_softbuffer_rx_reset_tbs(phy_ul_sched_res->npusch[n].softbuffer_rx,
          //            sched_result.npusch[i].tbs * 8);
          //          }

          phy_ul_sched_res->npusch[n].data = ue_db[rnti]->request_buffer(tti_tx_ul, sched_result.npusch[i].tbs);
          phy_ul_sched_res->nof_grants++;
          n++;

          // printf("SCHED: get_ul_sched: tbs=%d\n",sched_result.npusch[i].tbs);
        } else {
          Warning("Invalid UL scheduling result. User 0x%x does not exist\n", rnti);
        }

      } else {
        Warning("Grant %d for rnti=0x%x has zero TBS\n", i, sched_result.npusch[i].dci.rnti);
      }
    }

    // No more uses of ue_db beyond here
  }

  return SRSLTE_SUCCESS;
}

} // namespace sonica_enb
