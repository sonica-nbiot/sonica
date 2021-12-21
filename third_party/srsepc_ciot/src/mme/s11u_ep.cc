/*
 * Copyright 2021       Metro Group @ UCLA
 *
 * This file is part of Sonica.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srsepc_ciot/mme/s11u_ep.h"
#include "srsepc_ciot/mme/s1ap.h"
#include "srslte/upper/gtpu.h"
#include "srslte/upper/ipv6.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/ip.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace srsepc {

int s11u_ep::init(srslte::log_ref logger_)
{
  m_log = logger_;

  socklen_t sock_len;
  char      spgw_addr_name[] = "@spgw_s11u";
  char      mme_addr_name[]  = "@mme_s11u";

  // Logs
  m_log->info("Initializing MME S11-U interface.\n");

  // Open Socket
  m_s11u = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (m_s11u < 0) {
    m_log->error("Error opening UNIX socket. Error %s\n", strerror(errno));
    return SRSLTE_ERROR_CANT_START;
  }
  m_s11u_up = true;

  m_s1ap = s1ap::get_instance();

  // Set MME Address
  memset(&m_mme_addr, 0, sizeof(struct sockaddr_un));
  m_mme_addr.sun_family = AF_UNIX;
  snprintf(m_mme_addr.sun_path, sizeof(m_mme_addr.sun_path), "%s", mme_addr_name);
  m_mme_addr.sun_path[0] = '\0';

  // Set SPGW Address
  memset(&m_spgw_addr, 0, sizeof(struct sockaddr_un));
  m_spgw_addr.sun_family = AF_UNIX;
  snprintf(m_spgw_addr.sun_path, sizeof(m_spgw_addr.sun_path), "%s", spgw_addr_name);
  m_spgw_addr.sun_path[0] = '\0';

  // Bind socket to address
  if (bind(m_s11u, (const struct sockaddr*)&m_mme_addr, sizeof(m_mme_addr)) == -1) {
    m_log->error("Error binding UNIX socket. Error %s\n", strerror(errno));
    return SRSLTE_ERROR_CANT_START;
  }
  return SRSLTE_SUCCESS;
}

void s11u_ep::send_s11u_pdu(uint32_t mme_teid, srslte::byte_buffer_t* msg)
{
  printf("NB-IoT: s11u_ep::send_s11u_pdu------------------------------ Send packet to S11-U: Size %d\n", msg->N_bytes);
  for (uint32_t i = 0; i < msg->N_bytes;) {
    for (uint32_t j = 0; j < 16 && i < msg->N_bytes; i++, j++) {
      printf("%02x ", msg->msg[i]);
    }
    printf("\n");
  }
  printf("NB-IoT: s11u_ep::send_s11u_pdu---------- Packet End\n");

  // Check valid IP version
  struct iphdr* ip_pkt = (struct iphdr*)msg->msg;

  if (ip_pkt->version != 4 && ip_pkt->version != 6) {
    m_log->error("Invalid IP version to SPGW\n");
    m_log->console("Invalid IP version to SPGW\n");
    return;
  } else if (ip_pkt->version == 4) {
    if (ntohs(ip_pkt->tot_len) != msg->N_bytes) {
      m_log->error("IP Len and PDU N_bytes mismatch\n");
      m_log->console("IP Len and PDU N_bytes mismatch\n");
    }
    m_log->debug("S1-U PDU -- IP version %d, Total length %d\n", ip_pkt->version, ntohs(ip_pkt->tot_len));
    m_log->console("S1-U PDU -- IP version %d, Total length %d\n", ip_pkt->version, ntohs(ip_pkt->tot_len));
    m_log->debug("S1-U PDU -- IP src addr %s\n", srslte::gtpu_ntoa(ip_pkt->saddr).c_str());
    m_log->console("S1-U PDU -- IP src addr %s\n", srslte::gtpu_ntoa(ip_pkt->saddr).c_str());
    m_log->debug("S1-U PDU -- IP dst addr %s\n", srslte::gtpu_ntoa(ip_pkt->daddr).c_str());
    m_log->console("S1-U PDU -- IP dst addr %s\n", srslte::gtpu_ntoa(ip_pkt->daddr).c_str());
  }

  srslte::gtpu_header_t header;
  header.flags        = GTPU_FLAGS_VERSION_V1 | GTPU_FLAGS_GTP_PROTOCOL;
  header.message_type = GTPU_MSG_DATA_PDU;
  header.length       = msg->N_bytes;
  header.teid         = mme_teid;

  if (!gtpu_write_header(&header, msg, m_log)) {
    m_log->error("Error writing GTP-U Header. Flags 0x%x, Message Type 0x%x\n", header.flags, header.message_type);
    return;
  }

  if (sendto(m_s11u, msg->msg, msg->N_bytes, 0, (struct sockaddr*)&m_spgw_addr, sizeof(m_spgw_addr)) < 0) {
    perror("sendto");
  }
}

void s11u_ep::handle_s11u_pdu(srslte::byte_buffer_t* msg)
{
  srslte::gtpu_header_t header;
  srslte::gtpu_read_header(msg, &header, m_log);

  printf("NB-IoT: s11u_ep::handle_s11u_pdu------------------------------ Received packet from S11-U: Size %d\n", msg->N_bytes);
  for (uint32_t i = 0; i < msg->N_bytes;) {
    for (uint32_t j = 0; j < 16 && i < msg->N_bytes; i++, j++) {
      printf("%02x ", msg->msg[i]);
    }
    printf("\n");
  }

  uint32_t mme_ctrl_teid = header.teid;
  printf("NB-IoT: s11u_ep::handle_s11u_pdu-------------------------TEID from header is %d\n", mme_ctrl_teid);
  // TODO: The teid does not match, tmp hack
  mme_ctrl_teid                                  = 1;
  std::map<uint32_t, uint64_t>::iterator imsi_it = m_mme_ctr_teid_to_imsi->find(mme_ctrl_teid);
  if (imsi_it == m_mme_ctr_teid_to_imsi->end()) {
    printf("NB-IoT: s11u_ep::handle_s11u_pdu-------------------------Could not find IMSI from control TEID %d\n",
           mme_ctrl_teid);
  } else {
    printf("NB-IoT: s11u_ep::handle_s11u_pdu-------------------------find IMSI=%ld from control TEID %d\n",
           imsi_it->second,
           mme_ctrl_teid);
  }

  nas* nas_ctx = m_s1ap->find_nas_ctx_from_imsi(imsi_it->second);
//  printf("NB-IoT: handle_s11u_pdu------------- Receive User Pkt.\n");
//  m_s1ap->send_downlink_nas_transport(
//      nas_ctx->m_ecm_ctx.enb_ue_s1ap_id, nas_ctx->m_ecm_ctx.mme_ue_s1ap_id, msg, nas_ctx->m_ecm_ctx.enb_sri);
  nas_ctx->send_esm_data_transport(msg);
}

} // namespace srsepc
