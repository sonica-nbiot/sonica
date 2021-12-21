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

#include "sonica_enb/hdr/stack/rrc/rrc.h"
#include "srslte/asn1/asn1_utils.h"
#include "srslte/asn1/rrc_asn1_utils.h"
#include "srslte/asn1/rrc_asn1_nbiot.h"

using srslte::byte_buffer_t;

using namespace asn1::rrc;

namespace sonica_enb {

rrc::rrc() : rrc_log("RRC")
{
  // pending_paging.clear();
}

rrc::~rrc() {}

void rrc::init(const rrc_cfg_t&       cfg_,
               // phy_interface_rrc_lte* phy_,
               mac_interface_rrc*     mac_,
               rlc_interface_rrc*     rlc_,
                pdcp_interface_rrc*    pdcp_,
                s1ap_interface_rrc*    s1ap_,
               // gtpu_interface_rrc*    gtpu_,
               srslte::timer_handler* timers_)
{
  // phy    = phy_;
  mac    = mac_;
  rlc    = rlc_;
  pdcp   = pdcp_;
  // gtpu   = gtpu_;
  s1ap   = s1ap_;
  timers = timers_;

  pool = srslte::byte_buffer_pool::get_instance();

  cfg = cfg_;

  generate_sibs();
  // config_mac();

  running = true;
}

void rrc::stop()
{
  if (running) {
    running   = false;
    // rrc_pdu p = {0, LCID_EXIT, nullptr};
    // rx_pdu_queue.push(std::move(p));
  }
  // users.clear();
}

template <class T>
void rrc::log_rrc_message(const std::string&           source,
                          const direction_t            dir,
                          const srslte::byte_buffer_t* pdu,
                          const T&                     msg,
                          const std::string&           msg_type)
{
  if (rrc_log->get_level() == srslte::LOG_LEVEL_INFO) {
    rrc_log->info("%s - %s %s (%d B)\n", source.c_str(), dir == Tx ? "Tx" : "Rx", msg_type.c_str(), pdu->N_bytes);
  } else if (rrc_log->get_level() >= srslte::LOG_LEVEL_DEBUG) {
    asn1::json_writer json_writer;
    msg.to_json(json_writer);
    rrc_log->debug_hex(pdu->msg,
                       pdu->N_bytes,
                       "%s - %s %s (%d B)\n",
                       source.c_str(),
                       dir == Tx ? "Tx" : "Rx",
                       msg_type.c_str(),
                       pdu->N_bytes);
    rrc_log->debug_long("Content:\n%s\n", json_writer.to_string().c_str());
  }
}

/*******************************************************************************
  MAC interface

  Those functions that shall be called from a phch_worker should push the command
          to the queue and process later
*******************************************************************************/

void rrc::rem_user_thread(uint16_t rnti)
{
  rrc_pdu p = {rnti, LCID_REM_USER, nullptr};
  rx_pdu_queue.push(std::move(p));
}

// This function is called from PRACH worker (can wait)
void rrc::add_user(uint16_t rnti, const sched_interface::ue_cfg_t& sched_ue_cfg)
{
  auto user_it = users.find(rnti);
  if (user_it == users.end()) {
    bool rnti_added = true;
    if (rnti != SRSLTE_MRNTI) {
      // only non-eMBMS RNTIs are present in user map
      auto p = users.insert(std::make_pair(rnti, std::unique_ptr<ue>(new ue{this, rnti, sched_ue_cfg})));
      //      rnti_added = p.second and p.first->second->is_allocated();
    }
    if (rnti_added) {
      rlc->add_user(rnti);
      pdcp->add_user(rnti);
      rrc_log->info("Added new user rnti=0x%x\n", rnti);
      printf("RRC: Added new user rnti=0x%x\n", rnti);
    } else {
      //      mac->bearer_ue_rem(rnti, 0);
      rrc_log->error("Adding user rnti=0x%x - Failed to allocate user resources\n", rnti);
    }
  } else {
    rrc_log->error("Adding user rnti=0x%x (already exists)\n", rnti);
  }

  //  if (rnti == SRSLTE_MRNTI) {
  //    uint32_t teid_in = 1;
  //    for (auto& mbms_item : mcch.msg.c1().mbsfn_area_cfg_r9().pmch_info_list_r9[0].mbms_session_info_list_r9) {
  //      uint32_t lcid = mbms_item.lc_ch_id_r9;
  //
  //      // adding UE object to MAC for MRNTI without scheduling configuration (broadcast not part of regular
  //      scheduling) mac->ue_cfg(SRSLTE_MRNTI, NULL); rlc->add_bearer_mrb(SRSLTE_MRNTI, lcid);
  //      pdcp->add_bearer(SRSLTE_MRNTI, lcid, srslte::make_drb_pdcp_config_t(1, false));
  //      gtpu->add_bearer(SRSLTE_MRNTI, lcid, 1, 1, &teid_in);
  //    }
  //  }
}

/* Future: Add this for future updating user
 * Function called by MAC after the reception of a C-RNTI CE indicating that the UE still has a valid RNTI. */
void rrc::upd_user(uint16_t new_rnti, uint16_t old_rnti)
{
  // Remove new_rnti
  rem_user_thread(new_rnti);

  // Send Reconfiguration to old_rnti if is RRC_CONNECT or RRC Release if already released here
  auto old_it = users.find(old_rnti);
  if (old_it != users.end()) {
    //    if (old_it->second->is_connected()) {
    //      old_it->second->send_connection_reconf_upd(srslte::allocate_unique_buffer(*pool));
    //    } else {
    //      old_it->second->send_connection_release();
    //    }
  }
}

/*******************************************************************************
  PDCP interface
*******************************************************************************/
void rrc::write_pdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t pdu)
{
  rrc_pdu p = {rnti, lcid, std::move(pdu)};
  rx_pdu_queue.push(std::move(p));
}

/*******************************************************************************
  S1AP interface
*******************************************************************************/

uint8_t  nb_auth_response[] = {0x07, 0x53, 0x08, 0xA1, 0xD3, 0x03, 0xD8, 0x06, 0xFB, 0x9C, 0x22};
uint32_t nb_auth_response_len = sizeof(nb_auth_response);

uint8_t  nb_smc_complete[] = {0x07, 0x5E, 0x23, 0x09, 0x83, 0x66, 0x24, 0x05, 0x03, 0x21, 0x54, 0x04, 0xF6};
uint32_t nb_smc_complete_len = sizeof(nb_smc_complete);

uint8_t  esm_info_response[] = {0x02, 0x01, 0xDA, 0x28, 0x07, 0x03, 0x69, 0x6F, 0x74, 0x02, 0x6E, 0x62};
uint32_t esm_info_response_len = sizeof(esm_info_response);

uint8_t  nb_attach_complete[] = {0x07, 0x43, 0x00, 0x03, 0x52, 0x00, 0xC2};
uint32_t nb_attach_complete_len = sizeof(nb_attach_complete);

uint8_t real_nb_cp_service_request[] = {
    0x07, 0x4D, 0x20, 0x78, 0x00, 0x4C, 0x52, 0x00, 0xEB, 0x00, 0x47, 0x45, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0x11, 0xF9, 0x5C, 0x1E, 0x02, 0x94, 0x37, 0x08, 0x08, 0x08, 0x08, 0x8F, 0x73, 0x00, 0x35, 0x00,
    0x33, 0xB8, 0x0A, 0x56, 0x7B, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x65, 0x63,
    0x68, 0x6F, 0x10, 0x6D, 0x62, 0x65, 0x64, 0x63, 0x6C, 0x6F, 0x75, 0x64, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6E,
    0x67, 0x03, 0x63, 0x6F, 0x6D, 0x00, 0x00, 0x01, 0x00, 0x01, 0x57, 0x02, 0x20, 0x00,
};

uint8_t nb_cp_service_request_plain[] = {
    0x07, 0x4D, 0x20, 0x78, 0x00, 0x59, 0x52, 0x00, 0xEB, 0x00, 0x54, 0x45, 0x00, 0x00, 0x54, 0x80, 0xd2,
    0x40, 0x00, 0x40, 0x01, 0xfd, 0xb4, 0xac, 0x10, 0x00, 0x02, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x8c,
    0x11, 0x00, 0x01, 0x00, 0x04, 0x90, 0xd3, 0x32, 0x61, 0x00, 0x00, 0x00, 0x00, 0xa2, 0xb4, 0x06, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x02, 0x20, 0x00,
};

uint8_t nb_cp_service_request[] = {
    0x57, 0xCA, 0x74, 0x32, 0x02, 0x03, 0x07, 0x4D, 0x20, 0x78, 0x00, 0x59, 0x52, 0x00, 0xEB, 0x00, 0x54, 0x45,
    0x00, 0x00, 0x54, 0x80, 0xd2, 0x40, 0x00, 0x40, 0x01, 0xfd, 0xb4, 0xac, 0x10, 0x00, 0x02, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x00, 0x8c, 0x11, 0x00, 0x01, 0x00, 0x04, 0x90, 0xd3, 0x32, 0x61, 0x00, 0x00, 0x00, 0x00, 0xa2,
    0xb4, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x02, 0x20, 0x00,
};
uint32_t nb_cp_service_request_len = sizeof(nb_cp_service_request);

uint8_t  esm_data_transport[]   = {0x27, 0x00, 0x00, 0x00, 0x00, 0x04, 0x52, 0x00, 0xEB, 0x00, 0x30, 0x45,
                                0x00, 0x00, 0x30, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x06, 0xB2, 0x1B, 0x1E,
                                0x02, 0x94, 0x37, 0x34, 0xD7, 0x22, 0x9B, 0x7D, 0xCF, 0x00, 0x0D, 0x54,
                                0x61, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x02, 0x40, 0x00, 0x59,
                                0x5E, 0x00, 0x00, 0x02, 0x04, 0x05, 0x8C, 0x03, 0x03, 0x00, 0x00};
uint32_t esm_data_transport_len = sizeof(esm_data_transport);

void rrc::write_dl_info(uint16_t rnti, srslte::unique_byte_buffer_t sdu)
{
  dl_dcch_msg_nb_s dl_dcch_msg;
  dl_dcch_msg.msg.set_c1();
  dl_dcch_msg_type_nb_c::c1_c_* msg_c1 = &dl_dcch_msg.msg.c1();

  printf("RRC: write_dl_info------------Rx SDU for rnti=%x, length=%d\n",rnti, sdu->N_bytes);
  for(uint32_t i =0; i<sdu->N_bytes; i++){
    printf("0x%x ", sdu->msg[i]);
  }
  printf("\nRRC: write_dl_info------------Rx SDU End\n");

  auto user_it = users.find(rnti);
  if (user_it != users.end()) {

    dl_info_transfer_nb_r13_ies_s* dl_info_r13 =
        &msg_c1->set_dl_info_transfer_r13().crit_exts.set_c1().set_dl_info_transfer_r13();
    dl_info_r13->non_crit_ext_present      = false;
    dl_info_r13->late_non_crit_ext_present = false;
    //    dl_info_r13->ded_info_type.set_ded_info_nas();
    dl_info_r13->ded_info_nas_r13.resize(sdu->N_bytes);
    memcpy(dl_info_r13->ded_info_nas_r13.data(), sdu->msg, sdu->N_bytes);

    sdu->clear();

    user_it->second->send_dl_dcch(&dl_dcch_msg, std::move(sdu));

    // TODO: Add the EPC Test part 2 here
//     if(test_msg_counter == 0){
//       // Simulate the Auth Response
//       printf("RRC: --------------------------Trigger the NAS Auth Response\n");
//       srslte::byte_buffer_pool*    pool = srslte::byte_buffer_pool::get_instance();
//       srslte::unique_byte_buffer_t msg1;
//       msg1 = allocate_unique_buffer(*pool);
//
//       msg1->N_bytes = nb_auth_response_len;
//       memcpy(msg1->msg, nb_auth_response, nb_auth_response_len);
//       s1ap->write_pdu(rnti, std::move(msg1));
//
//       printf("RRC: --------------------------Send Auth Complete, test_msg_counter=%d\n", test_msg_counter++);
//     } else if (test_msg_counter==1){
//       // Simulate the Auth Complete
//       printf("RRC: --------------------------Trigger the NAS Secure Mode Command Complete\n");
//       srslte::byte_buffer_pool*    pool = srslte::byte_buffer_pool::get_instance();
//       srslte::unique_byte_buffer_t msg2;
//       msg2 = allocate_unique_buffer(*pool);
//
//       msg2->N_bytes = nb_smc_complete_len;
//       memcpy(msg2->msg, nb_smc_complete, nb_smc_complete_len);
//       s1ap->write_pdu(rnti, std::move(msg2));
//
//       printf("RRC: --------------------------Send Auth Complete, test_msg_counter=%d\n", test_msg_counter++);
//     } else if (test_msg_counter==2){
//       // Simulate ESM Information Response
//       printf("RRC: --------------------------Trigger the NAS ESM Information Response\n");
//       srslte::byte_buffer_pool*    pool = srslte::byte_buffer_pool::get_instance();
//       srslte::unique_byte_buffer_t msg2;
//       msg2 = allocate_unique_buffer(*pool);
//
//       msg2->N_bytes = esm_info_response_len;
//       memcpy(msg2->msg, esm_info_response, esm_info_response_len);
//       s1ap->write_pdu(rnti, std::move(msg2));
//
//       printf("RRC: --------------------------Send ESM Information Response, test_msg_counter=%d\n", test_msg_counter++);
//     } else if (test_msg_counter==3){
//       // Simulate Attach Complete
//       printf("RRC: --------------------------Trigger the NAS Attach Complete\n");
//       srslte::byte_buffer_pool*    pool = srslte::byte_buffer_pool::get_instance();
//       srslte::unique_byte_buffer_t msg2;
//       msg2 = allocate_unique_buffer(*pool);
//
//       msg2->N_bytes = nb_attach_complete_len;
//       memcpy(msg2->msg, nb_attach_complete, nb_attach_complete_len);
//       s1ap->write_pdu(rnti, std::move(msg2));
//
//       printf("RRC: --------------------------Send NAS Attach Complete, test_msg_counter=%d\n", test_msg_counter++);
//     } else if (test_msg_counter==4){
//       // Simulate Attach Complete
//       printf("RRC: --------------------------Trigger the NAS Control Plane Service Request\n");
//       srslte::byte_buffer_pool*    pool = srslte::byte_buffer_pool::get_instance();
//       srslte::unique_byte_buffer_t msg2;
//       msg2 = allocate_unique_buffer(*pool);
//
//       msg2->N_bytes = nb_cp_service_request_len;
//       memcpy(msg2->msg, nb_cp_service_request, nb_cp_service_request_len);
//
//       asn1::s1ap::rrc_establishment_cause_e s1ap_cause;
//       s1ap_cause.value = asn1::s1ap::rrc_establishment_cause_opts::options::mo_data;
//
//       uint8_t mmec = user_it->second->get_mmec();
//       uint32_t m_tmsi = user_it->second->get_mtmsi();
//       printf("RRC: --------------------------Control Plane Service Request, mmec=0x%x, m_tmsi=0x%x\n", mmec, m_tmsi);
//       s1ap->initial_ue(rnti, s1ap_cause, std::move(msg2), m_tmsi, mmec);
//
//       printf("RRC: --------------------------Send NAS Control Plane Service Request, test_msg_counter=%d\n", test_msg_counter++);
//     }
//    else if (test_msg_counter==5) {
//      // Simulate Attach Complete
//      printf("RRC: --------------------------Trigger the ESM Data Transport\n");
//      srslte::byte_buffer_pool*    pool = srslte::byte_buffer_pool::get_instance();
//      srslte::unique_byte_buffer_t msg2;
//      msg2 = allocate_unique_buffer(*pool);
//
//      msg2->N_bytes = esm_data_transport_len;
//      memcpy(msg2->msg, esm_data_transport, esm_data_transport_len);
//      s1ap->write_pdu(rnti, std::move(msg2));
//
//      printf("RRC: --------------------------Send ESM Data Transport, test_msg_counter=%d\n", test_msg_counter++);
//    }

  } else {
    rrc_log->error("Rx SDU for unknown rnti=0x%x\n", rnti);
  }
}

void rrc::release_complete(uint16_t rnti)
{
  rrc_pdu p = {rnti, LCID_REL_USER, nullptr};
  rx_pdu_queue.push(std::move(p));
}

bool rrc::setup_ue_ctxt(uint16_t rnti, const asn1::s1ap::init_context_setup_request_s& msg)
{
//  rrc_log->info("Adding initial context for 0x%x\n", rnti);
//  auto user_it = users.find(rnti);
//
//  if (user_it == users.end()) {
//    rrc_log->warning("Unrecognised rnti: 0x%x\n", rnti);
//    return false;
//  }
//
//  if (msg.protocol_ies.add_cs_fallback_ind_present) {
//    rrc_log->warning("Not handling AdditionalCSFallbackIndicator\n");
//  }
//  if (msg.protocol_ies.csg_membership_status_present) {
//    rrc_log->warning("Not handling CSGMembershipStatus\n");
//  }
//  if (msg.protocol_ies.gummei_id_present) {
//    rrc_log->warning("Not handling GUMMEI_ID\n");
//  }
//  if (msg.protocol_ies.ho_restrict_list_present) {
//    rrc_log->warning("Not handling HandoverRestrictionList\n");
//  }
//  if (msg.protocol_ies.management_based_mdt_allowed_present) {
//    rrc_log->warning("Not handling ManagementBasedMDTAllowed\n");
//  }
//  if (msg.protocol_ies.management_based_mdtplmn_list_present) {
//    rrc_log->warning("Not handling ManagementBasedMDTPLMNList\n");
//  }
//  if (msg.protocol_ies.mme_ue_s1ap_id_minus2_present) {
//    rrc_log->warning("Not handling MME_UE_S1AP_ID_2\n");
//  }
//  if (msg.protocol_ies.registered_lai_present) {
//    rrc_log->warning("Not handling RegisteredLAI\n");
//  }
//  if (msg.protocol_ies.srvcc_operation_possible_present) {
//    rrc_log->warning("Not handling SRVCCOperationPossible\n");
//  }
//  if (msg.protocol_ies.subscriber_profile_idfor_rfp_present) {
//    rrc_log->warning("Not handling SubscriberProfileIDforRFP\n");
//  }
//  if (msg.protocol_ies.trace_activation_present) {
//    rrc_log->warning("Not handling TraceActivation\n");
//  }
//  if (msg.protocol_ies.ue_radio_cap_present) {
//    rrc_log->warning("Not handling UERadioCapability\n");
//  }
//
//  // UEAggregateMaximumBitrate
//  user_it->second->set_bitrates(msg.protocol_ies.ueaggregate_maximum_bitrate.value);
//
//  // UESecurityCapabilities
//  user_it->second->set_security_capabilities(msg.protocol_ies.ue_security_cap.value);
//
//  // SecurityKey
//  user_it->second->set_security_key(msg.protocol_ies.security_key.value);
//
//  // CSFB
//  if (msg.protocol_ies.cs_fallback_ind_present) {
//    if (msg.protocol_ies.cs_fallback_ind.value.value == asn1::s1ap::cs_fallback_ind_opts::cs_fallback_required or
//        msg.protocol_ies.cs_fallback_ind.value.value == asn1::s1ap::cs_fallback_ind_opts::cs_fallback_high_prio) {
//      user_it->second->is_csfb = true;
//    }
//  }
//
//  // Send RRC security mode command
//  user_it->second->send_security_mode_command();
//
//  // Setup E-RABs
//  user_it->second->setup_erabs(msg.protocol_ies.erab_to_be_setup_list_ctxt_su_req.value);

  return true;
}

bool rrc::modify_ue_ctxt(uint16_t rnti, const asn1::s1ap::ue_context_mod_request_s& msg)
{
//  bool err = false;
//
//  rrc_log->info("Modifying context for 0x%x\n", rnti);
//  auto user_it = users.find(rnti);
//
//  if (user_it == users.end()) {
//    rrc_log->warning("Unrecognised rnti: 0x%x\n", rnti);
//    return false;
//  }
//
//  if (msg.protocol_ies.cs_fallback_ind_present) {
//    if (msg.protocol_ies.cs_fallback_ind.value.value == asn1::s1ap::cs_fallback_ind_opts::cs_fallback_required ||
//        msg.protocol_ies.cs_fallback_ind.value.value == asn1::s1ap::cs_fallback_ind_opts::cs_fallback_high_prio) {
//      /* Remember that we are in a CSFB right now */
//      user_it->second->is_csfb = true;
//    }
//  }
//
//  if (msg.protocol_ies.add_cs_fallback_ind_present) {
//    rrc_log->warning("Not handling AdditionalCSFallbackIndicator\n");
//    err = true;
//  }
//  if (msg.protocol_ies.csg_membership_status_present) {
//    rrc_log->warning("Not handling CSGMembershipStatus\n");
//    err = true;
//  }
//  if (msg.protocol_ies.registered_lai_present) {
//    rrc_log->warning("Not handling RegisteredLAI\n");
//  }
//  if (msg.protocol_ies.subscriber_profile_idfor_rfp_present) {
//    rrc_log->warning("Not handling SubscriberProfileIDforRFP\n");
//    err = true;
//  }
//
//  if (err) {
//    // maybe pass a cause value?
//    return false;
//  }
//
//  // UEAggregateMaximumBitrate
//  if (msg.protocol_ies.ueaggregate_maximum_bitrate_present) {
//    user_it->second->set_bitrates(msg.protocol_ies.ueaggregate_maximum_bitrate.value);
//  }
//
//  // UESecurityCapabilities
//  if (msg.protocol_ies.ue_security_cap_present) {
//    user_it->second->set_security_capabilities(msg.protocol_ies.ue_security_cap.value);
//  }
//
//  // SecurityKey
//  if (msg.protocol_ies.security_key_present) {
//    user_it->second->set_security_key(msg.protocol_ies.security_key.value);
//
//    // Send RRC security mode command ??
//    user_it->second->send_security_mode_command();
//  }

  return true;
}

bool rrc::setup_ue_erabs(uint16_t rnti, const asn1::s1ap::erab_setup_request_s& msg)
{
//  rrc_log->info("Setting up erab(s) for 0x%x\n", rnti);
//  auto user_it = users.find(rnti);
//
//  if (user_it == users.end()) {
//    rrc_log->warning("Unrecognised rnti: 0x%x\n", rnti);
//    return false;
//  }
//
//  if (msg.protocol_ies.ueaggregate_maximum_bitrate_present) {
//    // UEAggregateMaximumBitrate
//    user_it->second->set_bitrates(msg.protocol_ies.ueaggregate_maximum_bitrate.value);
//  }
//
//  // Setup E-RABs
//  user_it->second->setup_erabs(msg.protocol_ies.erab_to_be_setup_list_bearer_su_req.value);

  return true;
}

bool rrc::release_erabs(uint32_t rnti)
{
//  rrc_log->info("Releasing E-RABs for 0x%x\n", rnti);
//  auto user_it = users.find(rnti);
//
//  if (user_it == users.end()) {
//    rrc_log->warning("Unrecognised rnti: 0x%x\n", rnti);
//    return false;
//  }
//
//  bool ret = user_it->second->release_erabs();
//  return ret;
        return true;
}

uint8_t* rrc::read_pdu_bcch_dlsch(const uint32_t sib_index)
{
  if (sib_index == 0) {
    return sib_buffer1->msg;
  }

  return sib_buffer2->msg;
}

void rrc::tti_clock()
{
  rrc_pdu p;
  while (rx_pdu_queue.try_pop(&p)) {
    // print Rx PDU
    if (p.pdu != nullptr) {
      rrc_log->info_hex(p.pdu->msg, p.pdu->N_bytes, "Rx %s PDU", rb_id_text[p.lcid]);
      //      printf("RRC rrc::tti_clock() Rx %s PDU\n", rb_id_text[p.lcid]);
    }

    // check if user exists
    auto user_it = users.find(p.rnti);
    if (user_it == users.end()) {
      rrc_log->warning("Discarding PDU for removed rnti=0x%x\n", p.rnti);
      continue;
    }

    // handle queue cmd
    switch (p.lcid) {
      case RB_ID_SRB0:
        //        printf("RRC rrc::tti_clock() lcid=RB_ID_SRB0\n");
        parse_ul_ccch(p.rnti, std::move(p.pdu));
        break;
      case RB_ID_SRB1:
      case RB_ID_SRB1BIS:
        parse_ul_dcch(p.rnti, p.lcid, std::move(p.pdu));
        break;
      case LCID_REM_USER:
        rem_user(p.rnti);
        break;
      case LCID_REL_USER:
        process_release_complete(p.rnti);
        break;
      default:
        rrc_log->error("Rx PDU with invalid bearer id: %d\n", p.lcid);
        printf("Rx PDU with invalid bearer id: %d\n", p.lcid);
        break;
    }
  }
}

/*******************************************************************************
  Private functions
  All private functions are not mutexed and must be called from a mutexed environment
  from either a public function or the internal thread
*******************************************************************************/

void rrc::parse_ul_ccch(uint16_t rnti, srslte::unique_byte_buffer_t pdu)
{
  uint16_t old_rnti = 0;

  if (pdu) {
    ul_ccch_msg_nb_s  ul_ccch_msg;
    asn1::cbit_ref bref(pdu->msg, pdu->N_bytes);
    if (ul_ccch_msg.unpack(bref) != asn1::SRSASN_SUCCESS or
    ul_ccch_msg.msg.type().value != ul_ccch_msg_type_nb_c::types_opts::c1) {
      rrc_log->error("Failed to unpack UL-CCCH message\n");
      printf("Failed to unpack UL-CCCH message\n");
      return;
    }

    log_rrc_message("SRB0", Rx, pdu.get(), ul_ccch_msg, ul_ccch_msg.msg.c1().type().to_string());
    printf("RRC Type: %s\n",ul_ccch_msg.msg.c1().type().to_string().c_str());

    auto user_it = users.find(rnti);
    switch (ul_ccch_msg.msg.c1().type().value) {
      case ul_ccch_msg_type_c::c1_c_::types::rrc_conn_request:
        if (user_it != users.end()) {
          printf("RRC rrc::parse_ul_ccch ul_ccch_msg_type_c::c1_c_::types::rrc_conn_request\n");
          user_it->second->handle_rrc_con_req(&ul_ccch_msg.msg.c1().rrc_conn_request_r13());
        } else {
          rrc_log->error("Received ConnectionSetup for rnti=0x%x without context\n", rnti);
        }
        break;
      default:
        rrc_log->error("UL CCCH message not recognised\n");
        break;
    }
  }
}

///< User mutex must be hold by caller
void rrc::parse_ul_dcch(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t pdu)
{
  if (pdu) {
    auto user_it = users.find(rnti);
    if (user_it != users.end()) {
      user_it->second->parse_ul_dcch(lcid, std::move(pdu));
    } else {
      rrc_log->error("Processing %s: Unknown rnti=0x%x\n", rb_id_text[lcid], rnti);
    }
  }
}

///< User mutex must be hold by caller
void rrc::process_release_complete(uint16_t rnti)
{
  rrc_log->info("Received Release Complete rnti=0x%x\n", rnti);
  auto user_it = users.find(rnti);
  if (user_it != users.end()) {
    if (!user_it->second->is_idle()) {
      rlc->clear_buffer(rnti);
      user_it->second->send_connection_release();
      // There is no RRCReleaseComplete message from UE thus wait ~50 subframes for tx
      usleep(50000);
    }
    rem_user_thread(rnti);
  } else {
    rrc_log->error("Received ReleaseComplete for unknown rnti=0x%x\n", rnti);
  }
}

void rrc::rem_user(uint16_t rnti)
{
  auto user_it = users.find(rnti);
  if (user_it != users.end()) {
    rrc_log->console("RRC:NB-IOT: Disconnecting rnti=0x%x.\n", rnti);
    rrc_log->info("Disconnecting rnti=0x%x.\n", rnti);

    /* First remove MAC and GTPU to stop processing DL/UL traffic for this user */
    mac->ue_rem(rnti); // MAC handles PHY
    //    gtpu->rem_user(rnti);

    // Now remove RLC and PDCP
    rlc->rem_user(rnti);
    pdcp->rem_user(rnti);

    users.erase(rnti);
    rrc_log->info("Removed user rnti=0x%x\n", rnti);
    rrc_log->console("RRC: NB-IoT: Removed user rnti=0x%x\n", rnti);
  } else {
    rrc_log->error("Removing user rnti=0x%x (does not exist)\n", rnti);
    rrc_log->console("Removing user rnti=0x%x (does not exist)\n", rnti);
  }
}

void rrc::generate_sibs()
{
  asn1::dyn_array<bcch_dl_sch_msg_nb_s> msgs(2);
  msgs[0].msg.set_c1().set_sib_type1_r13() = cfg.sib1;

  msgs[1].msg.set_c1().set_sys_info_r13().crit_exts.set_sys_info_r13();
  asn1::rrc::sys_info_nb_r13_ies_s::sib_type_and_info_r13_l_& sib_list =
      msgs[1].msg.c1().sys_info_r13().crit_exts.sys_info_r13().sib_type_and_info_r13;

  asn1::rrc::sys_info_nb_r13_ies_s::sib_type_and_info_r13_item_c_ sibitem2;
  sibitem2.set_sib2_r13() = cfg.sib2;
  sib_list.push_back(sibitem2);

  asn1::rrc::sys_info_nb_r13_ies_s::sib_type_and_info_r13_item_c_ sibitem3;
  sibitem3.set_sib3_r13() = cfg.sib3;
  sib_list.push_back(sibitem3);

  sib_buffer1 = srslte::allocate_unique_buffer(*pool);
  asn1::bit_ref bref1(sib_buffer1->msg, sib_buffer1->get_tailroom());

  sib_buffer2 = srslte::allocate_unique_buffer(*pool);
  asn1::bit_ref bref2(sib_buffer2->msg, sib_buffer2->get_tailroom());

  if (msgs[0].pack(bref1) == asn1::SRSASN_ERROR_ENCODE_FAIL) {
    rrc_log->error("SIB1 encoding fail\n");
  }
  sib_buffer1->N_bytes = bref1.distance_bytes();
  // log_data(sib_buffer1->msg, sib_buffer1->N_bytes);

  if (msgs[1].pack(bref2) == asn1::SRSASN_ERROR_ENCODE_FAIL) {
    rrc_log->error("SIB2/3 encoding fail\n");
  }
  sib_buffer2->N_bytes = bref2.distance_bytes();
  // log_data(sib_buffer2->msg, sib_buffer2->N_bytes);
}

/*******************************************************************************
  UE class

  Every function in UE class is called from a mutex environment thus does not
  need extra protection.
*******************************************************************************/

rrc::ue::ue(rrc* outer_rrc, uint16_t rnti_, const sched_interface::ue_cfg_t& sched_ue_cfg) :
  parent(outer_rrc),
  rnti(rnti_),
  pool(srslte::byte_buffer_pool::get_instance()),
  current_sched_ue_cfg(sched_ue_cfg)
// phy_rrc_dedicated_list(sched_ue_cfg.supported_cc_list.size()),
// cell_ded_list(parent->cfg, *outer_rrc->pucch_res_list, *outer_rrc->cell_common_list)
{
  if (current_sched_ue_cfg.supported_cc_list.empty() or not current_sched_ue_cfg.supported_cc_list[0].active) {
    parent->rrc_log->warning("No PCell set. Picking eNBccIdx=0 as PCell\n");
    current_sched_ue_cfg.supported_cc_list.resize(1);
    current_sched_ue_cfg.supported_cc_list[0].active     = true;
    current_sched_ue_cfg.supported_cc_list[0].enb_cc_idx = UE_PCELL_CC_IDX;
  }

  //  activity_timer = outer_rrc->timers->get_unique_timer();
  //  set_activity_timeout(MSG3_RX_TIMEOUT); // next UE response is Msg3
  //  mobility_handler.reset(new rrc_mobility(this));
  //
  //  // Configure
  //  apply_setup_phy_common(parent->cfg.sibs[1].sib2().rr_cfg_common);
  //
  //  // Allocate cell and PUCCH resources
  //  if (cell_ded_list.add_cell(sched_ue_cfg.supported_cc_list[0].enb_cc_idx) == nullptr) {
  //    return;
  //  }
}

rrc::ue::~ue() {}

void rrc::ue::parse_ul_dcch(uint32_t lcid, srslte::unique_byte_buffer_t pdu)
{
  // FUTURE: The activity_timer is not added yet
  //  set_activity();

  ul_dcch_msg_nb_s ul_dcch_msg;
  asn1::cbit_ref   bref(pdu->msg, pdu->N_bytes);
  if (ul_dcch_msg.unpack(bref) != asn1::SRSASN_SUCCESS or
      ul_dcch_msg.msg.type().value != ul_dcch_msg_type_nb_c::types_opts::c1) {
    parent->rrc_log->error("Failed to unpack UL-DCCH message\n");
    return;
  }

  parent->log_rrc_message(rb_id_text[lcid], Rx, pdu.get(), ul_dcch_msg, ul_dcch_msg.msg.c1().type().to_string());

  // reuse PDU
  pdu->clear(); // TODO: name collision with byte_buffer reset

  transaction_id = 0;

  switch (ul_dcch_msg.msg.c1().type()) {
      //    Available Msg Types
      //    rrc_conn_recfg_complete_r13,
      //    rrc_conn_reest_complete_r13,
      //    rrc_conn_setup_complete_r13,
      //    security_mode_complete_r13,
      //    security_mode_fail_r13,
      //    ue_cap_info_r13,
      //    ul_info_transfer_r13,
      //    rrc_conn_resume_complete_r13,
    case ul_dcch_msg_type_nb_c::c1_c_::types::rrc_conn_setup_complete_r13:
      handle_rrc_con_setup_complete(&ul_dcch_msg.msg.c1().rrc_conn_setup_complete_r13(), std::move(pdu));
      break;
    case ul_dcch_msg_type_nb_c::c1_c_::types::ul_info_transfer_r13:
      pdu->N_bytes = ul_dcch_msg.msg.c1()
                         .ul_info_transfer_r13()
                         .crit_exts
                         .ul_info_transfer_r13()
                         .ded_info_nas_r13
                         .size();
      memcpy(pdu->msg,
             ul_dcch_msg.msg.c1()
                 .ul_info_transfer_r13()
                 .crit_exts
                 .ul_info_transfer_r13()
                 .ded_info_nas_r13
                 .data(),
             pdu->N_bytes);
      parent->s1ap->write_pdu(rnti, std::move(pdu));
      break;
      //    case ul_dcch_msg_type_c::c1_c_::types::meas_report:
      //      if (mobility_handler != nullptr) {
      //        mobility_handler->handle_ue_meas_report(ul_dcch_msg.msg.c1().meas_report());
      //      } else {
      //        parent->rrc_log->warning("Received MeasReport but no mobility configuration is available\n");
      //      }
      //      break;
    default:
      parent->rrc_log->error("Msg: %s not supported\n", ul_dcch_msg.msg.c1().type().to_string().c_str());
      break;
  }
}

void rrc::ue::handle_rrc_con_req(rrc_conn_request_nb_s* msg)
{
  //  if (not parent->s1ap->is_mme_connected()) {
  //    parent->rrc_log->error("MME isn't connected. Sending Connection Reject\n");
  //    send_connection_reject();
  //    return;
  //  }

  rrc_conn_request_nb_r13_ies_s* msg_r13 = &msg->crit_exts.rrc_conn_request_r13();

  if (msg_r13->ue_id_r13.type() == init_ue_id_c::types::s_tmsi) {
    mmec     = (uint8_t)msg_r13->ue_id_r13.s_tmsi().mmec.to_number();
    m_tmsi   = (uint32_t)msg_r13->ue_id_r13.s_tmsi().m_tmsi.to_number();
    has_tmsi = true;
    printf("RRC rrc::ue::handle_rrc_con_req s_tmsi: mmec=%x, m_tmsi=%x\n", mmec, m_tmsi);
  }else if(msg_r13->ue_id_r13.type() == init_ue_id_c::types::random_value){
    printf("RRC rrc::ue::handle_rrc_con_req random_value\n");
  }
  establishment_cause = msg_r13->establishment_cause_r13;
  send_connection_setup();
  state = RRC_STATE_WAIT_FOR_CON_SETUP_COMPLETE;

  //  set_activity_timeout(UE_RESPONSE_RX_TIMEOUT);
}

void rrc::ue::handle_rrc_con_setup_complete(rrc_conn_setup_complete_nb_s* msg, srslte::unique_byte_buffer_t pdu)
{
  // FUTURE: Inform PHY about the configuration completion
  //  parent->phy->complete_config_dedicated(rnti);

  parent->rrc_log->info("RRCConnectionSetupComplete transaction ID: %d\n", msg->rrc_transaction_id);
  //  rrc_conn_setup_complete_r8_ies_s* msg_r8 = &msg->crit_exts.c1().rrc_conn_setup_complete_r8();
  rrc_conn_setup_complete_nb_r13_ies_s* msg_r13 = &msg->crit_exts.rrc_conn_setup_complete_r13();

  // TODO: msg->selected_plmn_id - used to select PLMN from SIB1 list
  // TODO: if(msg->registered_mme_present) - the indicated MME should be used from a pool

  pdu->N_bytes = msg_r13->ded_info_nas_r13.size();
  memcpy(pdu->msg, msg_r13->ded_info_nas_r13.data(), pdu->N_bytes);

  // FUTURE: Acknowledge Dedicated Configuration
  //  parent->mac->phy_config_enabled(rnti, true);

  printf("RRC: handle_rrc_con_setup_complete--------------------------Send the rrc_con_setup_complete message\n");

  asn1::s1ap::rrc_establishment_cause_e s1ap_cause;
  s1ap_cause.value = (asn1::s1ap::rrc_establishment_cause_opts::options)establishment_cause.value;
  if (has_tmsi) {
    parent->s1ap->initial_ue(rnti, s1ap_cause, std::move(pdu), m_tmsi, mmec);
  } else {
    parent->s1ap->initial_ue(rnti, s1ap_cause, std::move(pdu));
  }
  state = RRC_STATE_WAIT_FOR_CON_RECONF_COMPLETE;
}

void rrc::ue::send_connection_setup(bool is_setup)
{
  dl_ccch_msg_nb_s dl_ccch_msg;
  dl_ccch_msg.msg.set_c1();

  rr_cfg_ded_nb_r13_s* rr_cfg = nullptr;
  if (is_setup) {
    dl_ccch_msg.msg.c1().set_rrc_conn_setup_r13();
    // Question: Is this the same as LTE?
    dl_ccch_msg.msg.c1().rrc_conn_setup_r13().rrc_transaction_id = (uint8_t)((transaction_id++) % 4);
    dl_ccch_msg.msg.c1().rrc_conn_setup_r13().crit_exts.set_c1().set_rrc_conn_setup_r13();
    rr_cfg = &dl_ccch_msg.msg.c1().rrc_conn_setup_r13().crit_exts.c1().rrc_conn_setup_r13().rr_cfg_ded_r13;
  } else {
    dl_ccch_msg.msg.c1().set_rrc_conn_reest_r13();
    dl_ccch_msg.msg.c1().rrc_conn_reest_r13().rrc_transaction_id = (uint8_t)((transaction_id++) % 4);
    dl_ccch_msg.msg.c1().rrc_conn_reest_r13().crit_exts.set_c1().set_rrc_conn_reest_r13();
    rr_cfg = &dl_ccch_msg.msg.c1().rrc_conn_reest_r13().crit_exts.c1().rrc_conn_reest_r13().rr_cfg_ded_r13;
  }

  // Add SRB1 to cfg
  rr_cfg->srb_to_add_mod_list_r13_present                  = true;
  rr_cfg->srb_to_add_mod_list_r13[0].lc_ch_cfg_r13_present = true;
  rr_cfg->srb_to_add_mod_list_r13[0].lc_ch_cfg_r13.set(srb_to_add_mod_nb_r13_s::lc_ch_cfg_r13_c_::types::default_value);
  rr_cfg->srb_to_add_mod_list_r13[0].rlc_cfg_r13_present = true;
  rr_cfg->srb_to_add_mod_list_r13[0].rlc_cfg_r13.set(srb_to_add_mod_nb_r13_s::rlc_cfg_r13_c_::types::default_value);

  // mac-MainConfig
  rr_cfg->mac_main_cfg_r13_present = true;
  mac_main_cfg_nb_r13_s* mac_cfg   = &rr_cfg->mac_main_cfg_r13.set_explicit_value_r13();
  mac_cfg->ul_sch_cfg_r13_present  = true;
  // Question: Init .mac_cnfg.ul_sch_cfg_r13 and lc_ch_sr_cfg_r13?
  mac_cfg->ul_sch_cfg_r13           = parent->cfg.mac_cnfg.ul_sch_cfg_r13;
  mac_cfg->lc_ch_sr_cfg_r13_present = true;
  mac_cfg->lc_ch_sr_cfg_r13         = parent->cfg.mac_cnfg.lc_ch_sr_cfg_r13;
  mac_cfg->time_align_timer_ded_r13 = parent->cfg.mac_cnfg.time_align_timer_ded_r13;

  // physicalConfigDedicated
  rr_cfg->phys_cfg_ded_r13_present    = true;
  phys_cfg_ded_nb_r13_s* phy_cfg      = &rr_cfg->phys_cfg_ded_r13;
  phy_cfg->npdcch_cfg_ded_r13_present = true;
  phy_cfg->npdcch_cfg_ded_r13         = parent->cfg.npdcch_cfg;

  // Power control
  phy_cfg->ul_pwr_ctrl_ded_r13_present          = true;
  phy_cfg->ul_pwr_ctrl_ded_r13.p0_ue_npusch_r13 = 0;

  // PUSCH
  phy_cfg->npusch_cfg_ded_r13_present = true;
  phy_cfg->npusch_cfg_ded_r13         = parent->cfg.npusch_cfg;

  // Carrier
  phy_cfg->carrier_cfg_ded_r13_present = false;
  phy_cfg->carrier_cfg_ded_r13         = parent->cfg.carrier_cfg;

  // Add SRB1 to Scheduler
  current_sched_ue_cfg.maxharq_tx              = 0; // Do not find corresponding cfg in NB-IoT, set it to 0 temporally
  current_sched_ue_cfg.continuous_pusch        = false;
  current_sched_ue_cfg.ue_bearers[0].direction = sonica_enb::sched_interface::ue_bearer_cfg_t::BOTH;
  current_sched_ue_cfg.ue_bearers[1].direction = sonica_enb::sched_interface::ue_bearer_cfg_t::BOTH;
  current_sched_ue_cfg.ue_bearers[3].direction = sonica_enb::sched_interface::ue_bearer_cfg_t::BOTH;

  // Configure MAC
  if (is_setup) {
    // In case of RRC Connection Setup message (Msg4), we need to resolve the contention by sending a ConRes CE
    parent->mac->ue_set_crnti(rnti, rnti, &current_sched_ue_cfg);
  } else {
    parent->mac->ue_cfg(rnti, &current_sched_ue_cfg);
  }

  // Configure SRB1 and SRB1bis in RLC
  parent->rlc->add_bearer(rnti, 1, srslte::rlc_config_t::srb_config(1));
  parent->rlc->add_bearer(rnti, 3, srslte::rlc_config_t::srb_config(3));

  // Configure SRB1 in PDCP
  parent->pdcp->add_bearer(rnti, 1, srslte::make_srb_pdcp_config_t(1, false));
  //
  //  // Configure PHY layer
  //  apply_setup_phy_config_dedicated(*phy_cfg); // It assumes SCell has not been set before
  //  parent->mac->phy_config_enabled(rnti, false);

  rr_cfg->drb_to_add_mod_list_r13_present   = false;
  rr_cfg->drb_to_release_list_r13_present   = false;
  rr_cfg->rlf_timers_and_consts_r13_present = false;
  //  rr_cfg->rlf_timers_and_consts_r13.set_present(false);

  send_dl_ccch(&dl_ccch_msg);
}

void rrc::ue::send_dl_ccch(dl_ccch_msg_nb_s* dl_ccch_msg)
{
  // Allocate a new PDU buffer, pack the message and send to PDCP
  srslte::unique_byte_buffer_t pdu = srslte::allocate_unique_buffer(*pool);
  if (pdu) {
    asn1::bit_ref bref(pdu->msg, pdu->get_tailroom());
    if (dl_ccch_msg->pack(bref) != asn1::SRSASN_SUCCESS) {
      parent->rrc_log->error_hex(pdu->msg, pdu->N_bytes, "Failed to pack DL-CCCH-Msg:\n");
      return;
    }
    pdu->N_bytes = 1u + (uint32_t)bref.distance_bytes(pdu->msg);

    char buf[32] = {};
    sprintf(buf, "SRB0 - rnti=0x%x", rnti);
    printf("RRC: rrc::ue::send_dl_ccch rnti=0x%x\n", rnti);
    parent->log_rrc_message(buf, Tx, pdu.get(), *dl_ccch_msg, dl_ccch_msg->msg.c1().type().to_string());
    parent->rlc->write_sdu(rnti, RB_ID_SRB0, std::move(pdu));
  } else {
    printf("RRC: rrc::ue::send_dl_ccch: Allocating pdu\n");
    parent->rrc_log->error("Allocating pdu\n");
  }
}

void rrc::ue::send_dl_dcch(dl_dcch_msg_nb_s* dl_dcch_msg, srslte::unique_byte_buffer_t pdu)
{
  if (!pdu) {
    pdu = srslte::allocate_unique_buffer(*pool);
  }
  if (pdu) {
    asn1::bit_ref bref(pdu->msg, pdu->get_tailroom());
    if (dl_dcch_msg->pack(bref) == asn1::SRSASN_ERROR_ENCODE_FAIL) {
      parent->rrc_log->error("Failed to encode DL-DCCH-Msg\n");
      return;
    }
    pdu->N_bytes = 1u + (uint32_t)bref.distance_bytes(pdu->msg);

    // send on RB_ID_SRB1BIS
    uint32_t lcid = RB_ID_SRB1BIS;

    char buf[32] = {};
    sprintf(buf, "SRB%d - rnti=0x%x", lcid, rnti);
    printf("NB-IoT: rrc::ue::send_dl_dcch---------------------------------lcid=%d - rnti=0x%hu, length=%d\n", lcid, rnti, pdu->N_bytes);
    parent->log_rrc_message(buf, Tx, pdu.get(), *dl_dcch_msg, dl_dcch_msg->msg.c1().type().to_string());

    // OLD LTE code use pdcp
    // parent->pdcp->write_sdu(rnti, lcid, std::move(pdu));

    //TODO: NB-IoT: direcly use RLC. Is this right?
    parent->rlc->write_sdu(rnti, lcid, std::move(pdu));
  } else {
    parent->rrc_log->error("Allocating pdu\n");
  }
}

bool rrc::ue::is_connected()
{
  return state == RRC_STATE_REGISTERED;
}

bool rrc::ue::is_idle()
{
  return state == RRC_STATE_IDLE;
}

void rrc::ue::send_connection_release()
{
  parent->rrc_log->debug("RRC: NB-IoT: -----------------------Send RRC Connection Release\n");
  printf("RRC: NB-IoT: -----------------------Send RRC Connection Release\n");
  dl_dcch_msg_nb_s dl_dcch_msg;
  dl_dcch_msg.msg.set_c1().set_rrc_conn_release_r13();
  dl_dcch_msg.msg.c1().rrc_conn_release_r13().rrc_transaction_id = (uint8_t)((transaction_id++) % 4);
  dl_dcch_msg.msg.c1().rrc_conn_release_r13().crit_exts.set_c1().set_rrc_conn_release_r13();
  dl_dcch_msg.msg.c1().rrc_conn_release_r13().crit_exts.c1().rrc_conn_release_r13().release_cause_r13 = release_cause_nb_r13_e::other;

  send_dl_dcch(&dl_dcch_msg);
}


} // namespace sonica_enb
