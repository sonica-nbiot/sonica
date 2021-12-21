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

#ifndef SRSENB_NB_RRC_H
#define SRSENB_NB_RRC_H

#include "rrc_config.h"
#include "rrc_metrics.h"
#include "srslte/common/block_queue.h"
#include "srslte/common/buffer_pool.h"
#include "srslte/common/common.h"
#include "srslte/common/logmap.h"
#include "srslte/interfaces/enb_interfaces_nb.h"

namespace sonica_enb {

class rrc final : public rrc_interface_pdcp,
                  public rrc_interface_mac,
                  public rrc_interface_rlc,
                  public rrc_interface_s1ap
{
public:
  rrc();
  ~rrc();

  void init(const rrc_cfg_t&       cfg_,
            // phy_interface_rrc_lte* phy,
            mac_interface_rrc*     mac,
            rlc_interface_rrc*     rlc,
            pdcp_interface_rrc*    pdcp,
            s1ap_interface_rrc*    s1ap,
            srslte::timer_handler* timers_);

  void stop();
  void tti_clock();

  // rrc_interface_mac
  uint8_t* read_pdu_bcch_dlsch(const uint32_t sib_index) override;
  void     add_user(uint16_t rnti, const sched_interface::ue_cfg_t& init_ue_cfg) override;
  void     upd_user(uint16_t new_rnti, uint16_t old_rnti) override;

  // rrc_interface_pdcp
  void write_pdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t pdu) override;

  // rrc_interface_s1ap
  void write_dl_info(uint16_t rnti, srslte::unique_byte_buffer_t sdu) override;
  void release_complete(uint16_t rnti) override;
  bool setup_ue_ctxt(uint16_t rnti, const asn1::s1ap::init_context_setup_request_s& msg) override;
  bool modify_ue_ctxt(uint16_t rnti, const asn1::s1ap::ue_context_mod_request_s& msg) override;
  bool setup_ue_erabs(uint16_t rnti, const asn1::s1ap::erab_setup_request_s& msg) override;
  bool release_erabs(uint32_t rnti) override;

  // logging
  typedef enum { Rx = 0, Tx } direction_t;
  template <class T>
  void log_rrc_message(const std::string&           source,
                       direction_t                  dir,
                       const srslte::byte_buffer_t* pdu,
                       const T&                     msg,
                       const std::string&           msg_type);

private:
  class ue
  {
  public:
    ue(rrc* outer_rrc, uint16_t rnti, const sched_interface::ue_cfg_t& ue_cfg);
    ~ue();
    bool is_connected();
    bool is_idle();

    typedef enum {
      MSG3_RX_TIMEOUT = 0,    ///< Msg3 has its own timeout to quickly remove fake UEs from random PRACHs
      UE_RESPONSE_RX_TIMEOUT, ///< General purpose timeout for responses to eNB requests
      UE_INACTIVITY_TIMEOUT,  ///< UE inactivity timeout
      nulltype
    } activity_timeout_type_t;

    uint16_t rnti   = 0;
    rrc*     parent = nullptr;

    void send_connection_setup(bool is_setup = true);

    void parse_ul_dcch(uint32_t lcid, srslte::unique_byte_buffer_t pdu);

    void handle_rrc_con_req(asn1::rrc::rrc_conn_request_nb_s* msg);
    void handle_rrc_con_setup_complete(asn1::rrc::rrc_conn_setup_complete_nb_s* msg, srslte::unique_byte_buffer_t pdu);

    void send_dl_ccch(asn1::rrc::dl_ccch_msg_nb_s* dl_ccch_msg);
    void send_dl_dcch(asn1::rrc::dl_dcch_msg_nb_s*    dl_dcch_msg,
                      srslte::unique_byte_buffer_t pdu = srslte::unique_byte_buffer_t());
    void send_connection_release();

    uint8_t get_mmec() {return mmec;}
    uint32_t get_mtmsi() {return m_tmsi;}


  private:
    // args
    srslte::byte_buffer_pool*           pool = nullptr;
    srslte::timer_handler::unique_timer activity_timer;

    // cached for ease of context transfer
    asn1::rrc::rrc_conn_recfg_s         last_rrc_conn_recfg;
    asn1::rrc::security_algorithm_cfg_s last_security_mode_cmd;

    asn1::rrc::establishment_cause_nb_r13_e establishment_cause;

    // S-TMSI for this UE
    bool     has_tmsi = false;
    uint32_t m_tmsi   = 0;
    uint8_t  mmec     = 0;

    // state
    sched_interface::ue_cfg_t current_sched_ue_cfg = {};
    uint32_t                  rlf_cnt              = 0;
    uint8_t                   transaction_id       = 0;
    rrc_state_t               state                = RRC_STATE_IDLE;

    std::map<uint32_t, asn1::rrc::srb_to_add_mod_s> srbs;
    std::map<uint32_t, asn1::rrc::drb_to_add_mod_s> drbs;

    uint8_t                      k_enb[32]; // Provided by MME
    srslte::as_security_config_t sec_cfg = {};

    asn1::s1ap::ue_aggregate_maximum_bitrate_s bitrates;
    asn1::s1ap::ue_security_cap_s              security_capabilities;
    bool                                       eutra_capabilities_unpacked = false;
    asn1::rrc::ue_eutra_cap_s                  eutra_capabilities;
    srslte::rrc_ue_capabilities_t              ue_capabilities;

    const static uint32_t UE_PCELL_CC_IDX = 0;
  }; // class ue

  // args
  srslte::timer_handler*    timers = nullptr;
  srslte::byte_buffer_pool* pool   = nullptr;
  // phy_interface_rrc_lte*    phy    = nullptr;
  mac_interface_rrc*        mac    = nullptr;
  rlc_interface_rrc*        rlc    = nullptr;
  pdcp_interface_rrc*       pdcp   = nullptr;
  // gtpu_interface_rrc*       gtpu   = nullptr;
  s1ap_interface_rrc*       s1ap   = nullptr;
  srslte::log_ref           rrc_log;

  uint32_t test_msg_counter = 0;

  std::map<uint16_t, std::unique_ptr<ue> >       users; // NOTE: has to have fixed addr

  void     process_release_complete(uint16_t rnti);
  void rem_user(uint16_t rnti);
  void generate_sibs();



  void parse_ul_dcch(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t pdu);
  void parse_ul_ccch(uint16_t rnti, srslte::unique_byte_buffer_t pdu);

  bool                         running         = false;
  static const int             RRC_THREAD_PRIO = 65;
  // srslte::block_queue<rrc_pdu> rx_pdu_queue;

  rrc_cfg_t cfg = {};

  srslte::unique_byte_buffer_t sib_buffer1;
  srslte::unique_byte_buffer_t sib_buffer2;

  typedef struct {
    uint16_t                     rnti;
    uint32_t                     lcid;
    srslte::unique_byte_buffer_t pdu;
  } rrc_pdu;

  const static uint32_t LCID_EXIT     = 0xffff0000;
  const static uint32_t LCID_REM_USER = 0xffff0001;
  const static uint32_t LCID_REL_USER = 0xffff0002;
  const static uint32_t LCID_RLF_USER = 0xffff0003;
  const static uint32_t LCID_ACT_USER = 0xffff0004;

  srslte::block_queue<rrc_pdu> rx_pdu_queue;

  void rem_user_thread(uint16_t rnti);
};

} // namespace sonica_enb

#endif // SRSENB_NB_RRC_H
