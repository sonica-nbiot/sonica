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

#include <bitset>
#include <inttypes.h>
#include <iostream>
#include <string.h>

#include "sonica_enb/hdr/stack/mac/ue.h"
#include "srslte/common/log_helper.h"
#include "srslte/interfaces/enb_interfaces_nb.h"

namespace sonica_enb {

ue::ue(uint16_t                 rnti_,
       sched_interface*         sched_,
//       rrc_interface_mac*       rrc_,
       rlc_interface_mac*       rlc_,
//       phy_interface_stack_nb* phy_,
       srslte::log_ref          log_,
       uint32_t                 nof_rx_harq_proc_,
       uint32_t                 nof_tx_harq_proc_) :
  rnti(rnti_),
  sched(sched_),
//  rrc(rrc_),
  rlc(rlc_),
//  phy(phy_),
  log_h(log_),
  mac_msg_dl(20, log_),
  mch_mac_msg_dl(10, log_),
  mac_msg_ul(20, log_),
  pdus(128),
  nof_rx_harq_proc(nof_rx_harq_proc_),
  nof_tx_harq_proc(nof_tx_harq_proc_),
  ta_fsm(this)
{
  srslte::byte_buffer_pool* pool = srslte::byte_buffer_pool::get_instance();
  tx_payload_buffer.resize(1);
  for (auto& carrier_buffers : tx_payload_buffer) {
    for (auto& harq_buffers : carrier_buffers) {
      for (srslte::unique_byte_buffer_t& tb_buffer : harq_buffers) {
        tb_buffer = srslte::allocate_unique_buffer(*pool);
      }
    }
  }

  pdus.init(this, log_h);

  // Allocate buffer for PCell
  allocate_cc_buffers();

  // Set LCID group for SRB0 and SRB1
//  set_lcg(0, 0);
//  set_lcg(1, 0);
}

ue::~ue()
{
  // Free up all softbuffers for all CCs
  for (auto buffer : softbuffer_rx) {
      srslte_softbuffer_rx_free(&buffer);
  }

  for (auto buffer : softbuffer_tx) {
      srslte_softbuffer_tx_free(&buffer);
  }
}

void ue::reset()
{
  nof_failures = 0;

  for (auto buffer : softbuffer_rx) {
      srslte_softbuffer_rx_reset(&buffer);
  }

  for (auto buffer : softbuffer_tx) {
      srslte_softbuffer_tx_reset(&buffer);
  }
}

uint32_t ue::allocate_cc_buffers(const uint32_t num_cc)
{
  for (uint32_t i = 0; i < num_cc; ++i) {
    // create and init Rx buffers for Pcell
    softbuffer_rx.resize(nof_rx_harq_proc);
    for (auto& buffer : softbuffer_rx) {
      srslte_softbuffer_rx_init(&buffer, nof_prb);
    }

    pending_buffers.resize(nof_rx_harq_proc);
    for (auto& buffer : pending_buffers) {
      buffer = nullptr;
    }

    // Create and init Tx buffers for Pcell
    softbuffer_tx.resize(nof_tx_harq_proc);
    for (auto& buffer : softbuffer_tx) {
      srslte_softbuffer_tx_init(&buffer, nof_prb);
    }
  }
  return softbuffer_tx.size();
}

uint8_t* ue::request_buffer(const uint32_t tti, const uint32_t len)
{
  uint8_t* ret = nullptr;
  if (len > 0) {
    if (!pending_buffers.at(tti % nof_rx_harq_proc)) {
      ret                                        = pdus.request(len);
      pending_buffers.at(tti % nof_rx_harq_proc) = ret;
    } else {
      log_h->error("Requesting buffer for pid %d, not pushed yet\n", tti % nof_rx_harq_proc);
    }
  } else {
    log_h->warning("Requesting buffer for zero bytes\n");
  }
  return ret;
}

bool ue::process_pdus()
{
  return pdus.process_pdus();
}

void ue::set_tti(uint32_t tti)
{
  last_tti = tti;
}

void ue::deallocate_pdu(const uint32_t tti)
{
  if (pending_buffers.at(tti % nof_rx_harq_proc)) {
    pdus.deallocate(pending_buffers.at(tti % nof_rx_harq_proc));
    pending_buffers.at(tti % nof_rx_harq_proc) = nullptr;
  } else {
    log_h->console("Error deallocating buffer for pid=%d. Not requested\n", tti % nof_rx_harq_proc);
  }
}

#include <assert.h>

void ue::process_pdu(uint8_t* pdu, uint32_t nof_bytes, srslte::pdu_queue::channel_t channel)
{
  printf("MAC UE process_pdu: length=%d, pdu=%x %x %x\n",nof_bytes, pdu[0], pdu[1], pdu[2]);

  // Store the DPR byte
  uint8_t dpr = 0x00;
  // NB-IoT Specific Processing for DPR control element: if lcid ==0, drop the second byte
  if((pdu[0]&0x1F)==0){
    nof_bytes -= 1;
    dpr = pdu[1];
    pdu[1] = pdu[0];
    pdu++;
  }
  // Unpack ULSCH MAC PDU
  mac_msg_ul.init_rx(nof_bytes, true);
  mac_msg_ul.parse_packet(pdu);

  //  if (pcap) {
  //    pcap->write_ul_crnti(pdu, nof_bytes, rnti, true, last_tti, UL_CC_IDX);
  //  }

  pdus.deallocate(pdu);

  /* Process CE after all SDUs because we need to update BSR after */
  bool bsr_received = false;
  while (mac_msg_ul.next()) {
    assert(mac_msg_ul.get());
    if (!mac_msg_ul.get()->is_sdu()) {
      // Process MAC Control Element
      bsr_received |= process_ce(mac_msg_ul.get());
    }
  }

  mac_msg_ul.reset();

  uint32_t lcid_most_data = 0;
  int      most_data      = -99;

  while (mac_msg_ul.next()) {
    assert(mac_msg_ul.get());
    if (mac_msg_ul.get()->is_sdu()) {
      // Route logical channel
      log_h->debug_hex(mac_msg_ul.get()->get_sdu_ptr(),
                       mac_msg_ul.get()->get_payload_size(),
                       "PDU:   rnti=0x%x, lcid=%d, %d bytes\n",
                       rnti,
                       mac_msg_ul.get()->get_sdu_lcid(),
                       mac_msg_ul.get()->get_payload_size());
      printf("Receive PDU: rnti=0x%x, lcid=%d, %d bytes\n",
             rnti,
             mac_msg_ul.get()->get_sdu_lcid(),
             mac_msg_ul.get()->get_payload_size());

      /* In some cases, an uplink transmission with only CQI has all zeros and gets routed to RRC
       * Compute the checksum if lcid=0 and avoid routing in that case
       */
      bool route_pdu = true;
      if (mac_msg_ul.get()->get_sdu_lcid() == 0) {
        uint8_t* x   = mac_msg_ul.get()->get_sdu_ptr();
        uint32_t sum = 0;
        for (uint32_t i = 0; i < mac_msg_ul.get()->get_payload_size(); i++) {
          sum += x[i];
        }
        if (sum == 0) {
          route_pdu = false;
          Warning("Received all zero PDU\n");
        }
      }

      if (route_pdu && rlc) {
        rlc->write_pdu(rnti,
                       mac_msg_ul.get()->get_sdu_lcid(),
                       mac_msg_ul.get()->get_sdu_ptr(),
                       mac_msg_ul.get()->get_payload_size());
      } else {
        printf("MAC UE mac_msg_ul.get()->get_sdu_ptr(): pdu=%x %x %x\n", mac_msg_ul.get()->get_sdu_ptr()[0], mac_msg_ul.get()->get_sdu_ptr()[1], mac_msg_ul.get()->get_sdu_ptr()[2]);
        printf("Sched UE: RLC is not set!\n");
      }

      // Indicate scheduler to update BSR counters
      // sched->ul_recv_len(rnti, mac_msg_ul.get()->get_sdu_lcid(), mac_msg_ul.get()->get_payload_size());

      // Indicate RRC about successful activity if valid RLC message is received
      if (mac_msg_ul.get()->get_payload_size() > 64) { // do not count RLC status messages only
                                                       //        rrc->set_activity_user(rnti);
        log_h->debug("UL activity rnti=0x%x, n_bytes=%d\n", rnti, nof_bytes);
      }

      if ((int)mac_msg_ul.get()->get_payload_size() > most_data) {
        most_data      = (int)mac_msg_ul.get()->get_payload_size();
        lcid_most_data = mac_msg_ul.get()->get_sdu_lcid();
      }

      // Save contention resolution if lcid == 0
      if (mac_msg_ul.get()->get_sdu_lcid() == 0 && route_pdu) {
        int nbytes = srslte::sch_subh::MAC_CE_CONTRES_LEN;
        if (mac_msg_ul.get()->get_payload_size() >= (uint32_t)nbytes) {
          uint8_t* ue_cri_ptr = (uint8_t*)&conres_id;
          uint8_t* pkt_ptr    = mac_msg_ul.get()->get_sdu_ptr();
          for (int i = 0; i < nbytes; i++) {
            ue_cri_ptr[nbytes - i - 1] = pkt_ptr[i];
          }
        } else {
          Error("Received CCCH UL message of invalid size=%d bytes\n", mac_msg_ul.get()->get_payload_size());
        }
      }
    }
  }

  // Process the DPR. The data valume is according to TS 36.321 Table 6.1.3.10-1a. Use tmp 125 bytes now.
  if(dpr != 0x00){
    printf("MAC: ue::process_pdu: DPR=%x, lcid_most_data=%d\n", dpr, lcid_most_data);
    sched->ul_bsr(rnti, lcid_most_data, 125, false);
    sched->ul_wait_timer(rnti, 30, true);
  }

  // If BSR is not received means that new data has arrived and there is no space for BSR transmission
  if (!bsr_received && lcid_most_data > 2) {
    // Add BSR to the LCID for which most data was received
    sched->ul_bsr(rnti, lcid_most_data, 125, false); // false adds BSR instead of setting
    Debug("BSR not received. Giving extra dci\n");
  }

  Debug("MAC PDU processed\n");
}

void ue::push_pdu(const uint32_t tti, uint32_t len)
{
  if (pending_buffers.at(tti % nof_rx_harq_proc)) {
    pdus.push(pending_buffers.at(tti % nof_rx_harq_proc), len);
    pending_buffers.at(tti % nof_rx_harq_proc) = nullptr;
  } else {
    log_h->console("Error pushing buffer for pid=%d. Not requested\n", tti % nof_rx_harq_proc);
  }
}

bool ue::process_ce(srslte::sch_subh* subh)
{
  uint32_t buff_size[4] = {0, 0, 0, 0};
  float    phr          = 0;
  int32_t  idx          = 0;
  uint16_t old_rnti     = 0;
  bool     is_bsr       = false;
  switch (subh->ul_sch_ce_type()) {
      //    case srslte::ul_sch_lcid::PHR_REPORT:
      //      phr = subh->get_phr();
      //      Info("CE:    Received PHR from rnti=0x%x, value=%.0f\n", rnti, phr);
      //      sched->ul_phr(rnti, (int)phr);
      //      metrics_phr(phr);
      //      break;
    case srslte::ul_sch_lcid::CRNTI:
      old_rnti = subh->get_c_rnti();
      Info("CE:    Received C-RNTI from temp_rnti=0x%x, rnti=0x%x\n", rnti, old_rnti);
      printf("CE:    Received C-RNTI from temp_rnti=0x%x, rnti=0x%x\n", rnti, old_rnti);
      if (sched->ue_exists(old_rnti)) {
        // rrc->upd_user(rnti, old_rnti);
        rnti = old_rnti;
      } else {
        Error("Updating user C-RNTI: rnti=0x%x already released\n", old_rnti);
      }
      break;
    case srslte::ul_sch_lcid::TRUNC_BSR:
    case srslte::ul_sch_lcid::SHORT_BSR:
      idx = subh->get_bsr(buff_size);
      if (idx == -1) {
        Error("Invalid Index Passed to lc groups\n");
        printf("Invalid Index Passed to lc groups\n");
        break;
      }
      //      for (uint32_t i = 0; i < lc_groups[idx].size(); i++) {
      //        // Indicate BSR to scheduler
      //        sched->ul_bsr(rnti, lc_groups[idx][i], buff_size[idx]);
      //      }
      // TODO: Currently directly set the bsr for LCID 3, ul_delay_timer=0(by default)
      sched->ul_bsr(rnti, 3, buff_size[idx]);

      Info("CE:    Received %s BSR rnti=0x%x, lcg=%d, value=%d\n",
           subh->ul_sch_ce_type() == srslte::ul_sch_lcid::SHORT_BSR ? "Short" : "Trunc",
           rnti,
           idx,
           buff_size[idx]);
      printf("MAC: NB-IoT: ue::process_ce: CE: Received %s BSR rnti=0x%x, lcg=%d, value=%d\n",
             subh->ul_sch_ce_type() == srslte::ul_sch_lcid::SHORT_BSR ? "Short" : "Trunc",
             rnti,
             idx,
             buff_size[idx]);
      is_bsr = true;
      break;
    case srslte::ul_sch_lcid::LONG_BSR:
      subh->get_bsr(buff_size);
      for (idx = 0; idx < 4; idx++) {
        for (uint32_t i = 0; i < lc_groups[idx].size(); i++) {
          sched->ul_bsr(rnti, lc_groups[idx][i], buff_size[idx]);
        }
      }
      is_bsr = true;
      Info("CE:    Received Long BSR rnti=0x%x, value=%d,%d,%d,%d\n",
           rnti,
           buff_size[0],
           buff_size[1],
           buff_size[2],
           buff_size[3]);
      break;
    case srslte::ul_sch_lcid::PADDING:
      Debug("CE:    Received padding for rnti=0x%x\n", rnti);
      break;
    default:
      Error("CE:    Invalid lcid=0x%x\n", (int)subh->ul_sch_ce_type());
      break;
  }
  return is_bsr;
}

int ue::read_pdu(uint32_t lcid, uint8_t* payload, uint32_t requested_bytes)
{
  return rlc->read_pdu(rnti, lcid, payload, requested_bytes);
}

void ue::allocate_sdu(srslte::sch_pdu* pdu, uint32_t lcid, uint32_t total_sdu_len)
{
  int sdu_space = pdu->get_sdu_space();
  printf("MAC allocate_sdu SDU: total_sdu_len=%d, sdu_space=%d\n", total_sdu_len, (uint32_t)sdu_space);
  if (sdu_space > 0) {
    int sdu_len = SRSLTE_MIN(total_sdu_len, (uint32_t)sdu_space);
    int n       = 1;
    while (sdu_len >= 2 && n > 0) { // minimum size is a single RLC AM status PDU (2 Byte)
      if (pdu->new_subh()) {        // there is space for a new subheader
        log_h->debug("SDU:   set_sdu(), lcid=%d, sdu_len=%d, sdu_space=%d\n", lcid, sdu_len, sdu_space);
        n = pdu->get()->set_sdu(lcid, sdu_len, this);
        if (n > 0) { // new SDU could be added
          sdu_len -= n;
          log_h->debug("SDU:   rnti=0x%x, lcid=%d, nbytes=%d, rem_len=%d\n", rnti, lcid, n, sdu_len);
        } else {
          Debug("Could not add SDU lcid=%d nbytes=%d, space=%d\n", lcid, sdu_len, sdu_space);
          pdu->del_subh();
        }
      } else {
        n = 0;
      }
    }
  }
}

void ue::allocate_ce(srslte::sch_pdu* pdu, uint32_t lcid)
{
  switch ((srslte::dl_sch_lcid)lcid) {
    case srslte::dl_sch_lcid::CON_RES_ID:
      if (pdu->new_subh()) {
        if (pdu->get()->set_con_res_id(conres_id)) {
          Info("CE:    Added Contention Resolution ID=0x%" PRIx64 "\n", conres_id);
        } else {
          Error("CE:    Setting Contention Resolution ID CE\n");
        }
      } else {
        Error("CE:    Setting Contention Resolution ID CE. No space for a subheader\n");
      }
      break;
    default:
      Error("CE:    Allocating CE=0x%x. Not supported\n", lcid);
      break;
  }
}

uint8_t* ue::generate_pdu(uint32_t                        ue_cc_idx,
                          uint32_t                        harq_pid,
                          uint32_t                        tb_idx,
                          sched_interface::dl_sched_pdu_t pdu[sched_interface::MAX_RLC_PDU_LIST],
                          uint32_t                        nof_pdu_elems,
                          uint32_t                        grant_size)
{
  std::lock_guard<std::mutex> lock(mutex);
  uint8_t*                    ret = nullptr;
  if (rlc) {
    if (ue_cc_idx < SRSLTE_MAX_CARRIERS && harq_pid < SRSLTE_FDD_NOF_HARQ && tb_idx < SRSLTE_MAX_TB) {
      tx_payload_buffer[ue_cc_idx][harq_pid][tb_idx]->clear();
      mac_msg_dl.init_tx(tx_payload_buffer[ue_cc_idx][harq_pid][tb_idx].get(), grant_size, false);
      for (uint32_t i = 0; i < nof_pdu_elems; i++) {
        if (pdu[i].lcid <= (uint32_t)srslte::ul_sch_lcid::PHR_REPORT) {
          allocate_sdu(&mac_msg_dl, pdu[i].lcid, pdu[i].nbytes);
        } else {
          allocate_ce(&mac_msg_dl, pdu[i].lcid);
        }
      }
      ret = mac_msg_dl.write_packet(log_h);
    } else {
      log_h->error(
          "Invalid parameters calling generate_pdu: cc_idx=%d, harq_pid=%d, tb_idx=%d\n", ue_cc_idx, harq_pid, tb_idx);
    }
  } else {
    std::cout << "Error ue not configured (must call config() first" << std::endl;
  }
  return ret;
}

/******* METRICS interface ***************/
void ue::metrics_rx(bool crc, uint32_t tbs)
{
  // if (crc) {
  //   metrics.rx_brate += tbs * 8;
  // } else {
  //   metrics.rx_errors++;
  // }
  // metrics.rx_pkts++;
}

} // namespace sonica_enb
