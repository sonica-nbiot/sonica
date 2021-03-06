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

#ifndef SRSLTE_NB_SCHEDULER_COMMON_H
#define SRSLTE_NB_SCHEDULER_COMMON_H

#include "srslte/common/bounded_bitset.h"
#include "srslte/interfaces/sched_interface_nb.h"

namespace sonica_enb {

//! Struct used to store possible CCE locations.
struct sched_dci_cce_t {
  uint32_t cce_start[4][6]; ///< Stores starting CCE for each aggr level index and CCE location index
  uint32_t nof_loc[4];      ///< Number of possible CCE locations for each aggregation level index
};

//! Params relative to a single TTI
struct tti_params_t {
  uint32_t tti_rx;
  uint32_t tti_tx_dl;
  uint32_t tti_tx_ul;
  uint32_t sf_idx_tx_dl;
  uint32_t sfn_tx_dl;
  explicit tti_params_t(uint32_t tti_rx_);
  uint32_t tti_rx_ack_dl() const { return tti_tx_ul; }
};

//! structs to bundle together all the sched arguments, and share them with all the sched sub-components
class sched_cell_params_t
{
  struct regs_deleter {
    void operator()(srslte_regs_t* p);
  };

public:
//  bool set_cfg(uint32_t                             enb_cc_idx_,
//               const sched_interface::cell_cfg_t&   cfg_,
//               const sched_interface::sched_args_t& sched_args);
//  // convenience getters
//  uint32_t prb_to_rbg(uint32_t nof_prbs) const { return (nof_prbs + (P - 1)) / P; }
//  uint32_t nof_prb() const { return cfg.cell.nof_prb; }
//
//  uint32_t                                       enb_cc_idx = 0;
//  sched_interface::cell_cfg_t                    cfg        = {};
//  const sched_interface::sched_args_t*           sched_cfg  = nullptr;
//  std::unique_ptr<srslte_regs_t, regs_deleter>   regs;
//  std::array<sched_dci_cce_t, 3>                 common_locations = {};
//  std::array<std::array<sched_dci_cce_t, 10>, 3> rar_locations    = {};
//  std::array<uint32_t, 3>                        nof_cce_table    = {}; ///< map cfix -> nof cces in PDCCH
//  uint32_t                                       P                = 0;
//  uint32_t                                       nof_rbgs         = 0;
};

//! Bitmask used for CCE allocations
using pdcch_mask_t = srslte::bounded_bitset<sched_interface::max_cce, true>;

//! Bitmask that stores the allocared DL RBGs
using rbgmask_t = srslte::bounded_bitset<1, true>; // JH: Set the rbg size to 1

//! Bitmask that stores the allocated UL PRBs
using prbmask_t = srslte::bounded_bitset<1, true>; // JH: Set the prb size to 1

//! Struct to express a {min,...,max} range of RBGs
struct prb_range_t;
struct rbg_range_t {
  uint32_t rbg_min = 0, rbg_max = 0;
  rbg_range_t() = default;
  rbg_range_t(uint32_t s, uint32_t e) : rbg_min(s), rbg_max(e) {}
  uint32_t nof_rbgs() const { return rbg_max - rbg_min; }

  static rbg_range_t prbs_to_rbgs(const prb_range_t& prbs, uint32_t P);
};

//! Struct to express a {min,...,max} range of PRBs
struct prb_range_t {
  uint32_t prb_min = 0, prb_max = 0;
  prb_range_t() = default;
  prb_range_t(uint32_t s, uint32_t e) : prb_min(s), prb_max(e) {}
  uint32_t nof_prbs() { return prb_max - prb_min; }

  static prb_range_t rbgs_to_prbs(const rbg_range_t& rbgs, uint32_t P);
  static prb_range_t riv_to_prbs(uint32_t riv, uint32_t nof_prbs, int nof_vrbs = -1);
};

/***********************
 *   Helper Functions
 **********************/

namespace sched_utils {

inline uint32_t aggr_level(uint32_t aggr_idx)
{
  return 1u << aggr_idx;
}

//! Obtain rvidx from nof retxs. This value is stored in DCI
inline uint32_t get_rvidx(uint32_t retx_idx)
{
  const static uint32_t rv_idx[4] = {0, 2, 3, 1};
  return rv_idx[retx_idx % 4];
}

//! Obtain nof retxs from rvidx.
inline uint32_t get_nof_retx(uint32_t rv_idx)
{
  const static uint32_t nof_retxs[4] = {0, 3, 1, 2};
  return nof_retxs[rv_idx % 4];
}

/**
 * Generate possible CCE locations a user can use to allocate DCIs
 * @param regs Regs data for the given cell configuration
 * @param location Result of the CCE location computation.
 * @param cfi Number of control symbols used for the PDCCH
 * @param sf_idx subframe index specific to the tx TTI (relevant only for data and RAR transmissions)
 * @param rnti identity of the user (invalid RNTI for RAR and BC transmissions)
 */
void generate_cce_location(srslte_regs_t*   regs,
                           sched_dci_cce_t* location,
                           uint32_t         cfi,
                           uint32_t         sf_idx = 0,
                           uint16_t         rnti   = SRSLTE_INVALID_RNTI);

} // namespace sched_utils

} // namespace sonica_enb

#endif // SRSLTE_NB_SCHEDULER_COMMON_H
