#ifndef SRSEPC_NMME_GW_H
#define SRSEPC_NMME_GW_H

#include "srslte/common/buffer_pool.h"
#include "srslte/common/common.h"
#include "srslte/common/interfaces_common.h"
#include "srslte/common/logmap.h"
#include <net/if.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace srsepc {

class s1ap;

class s11u_ep
{
public:
  s11u_ep() = default;

  int  init(srslte::log_ref logger_);
  void stop();

  int get_s11u();

  void handle_s11u_pdu(srslte::byte_buffer_t* msg);
  void send_s11u_pdu(uint32_t mme_teid, srslte::byte_buffer_t* msg);

  bool        m_s11u_up;
  int         m_s11u;
  struct sockaddr_un m_spgw_addr, m_mme_addr;

  void set_teid_to_imsi_map(std::map<uint32_t, uint64_t> *mme_ctr_teid_to_imsi){
    m_mme_ctr_teid_to_imsi = mme_ctr_teid_to_imsi;
  }

private:
  srslte::log_ref m_log;
  s1ap*  m_s1ap;
  std::map<uint32_t, uint64_t>*        m_mme_ctr_teid_to_imsi;
};

inline int s11u_ep::get_s11u()
{
  return m_s11u;
}



} // namespace srsepc

#endif // SRSEPC_NMME_GW_H