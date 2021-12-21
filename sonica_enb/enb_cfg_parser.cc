/*
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

#include "enb_cfg_parser.h"
#include "sonica_enb/hdr/enb_nb.h"
#include "srslte/asn1/liblte_common.h"
#include "srslte/asn1/rrc_asn1_utils.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/srslte.h"
// #include <boost/algorithm/string.hpp>


#define HANDLEPARSERCODE(cond)                                                                                         \
  do {                                                                                                                 \
    if ((cond) != 0) {                                                                                                 \
      printf("[%d][%s()] Parser Error detected\n", __LINE__, __FUNCTION__);                                            \
      return -1;                                                                                                       \
    }                                                                                                                  \
  } while (0)

#define COND_PARSER_WARN(cond, msg_fmt, ...)                                                                           \
  do {                                                                                                                 \
    if (cond) {                                                                                                        \
      printf(msg_fmt, ##__VA_ARGS__);                                                                                  \
    }                                                                                                                  \
  } while (0)

using namespace asn1::rrc;

namespace sonica_enb {

int field_sched_info::parse(libconfig::Setting& root)
{
  data->sched_info_list_r13.resize((uint32_t)root.getLength());
  for (uint32_t i = 0; i < data->sched_info_list_r13.size(); i++) {
    if (not parse_enum_by_number(data->sched_info_list_r13[i].si_periodicity_r13, "si_periodicity_r13", root[i])) {
      fprintf(stderr, "Missing field si_periodicity_r13 in sched_info=%d\n", i);
      return -1;
    }
    if (not parse_enum_by_str(data->sched_info_list_r13[i].si_repeat_pattern_r13, "si_repeat_pattern_r13", root[i])) {
      fprintf(stderr, "Missing/invalid field si_repeat_pattern_r13 in sched_info=%d\n", i);
      return -1;
    }
    if (root[i].exists("si_mapping_info_r13")) {
      data->sched_info_list_r13[i].sib_map_info_r13.resize((uint32_t)root[i]["si_mapping_info_r13"].getLength());
      if (data->sched_info_list_r13[i].sib_map_info_r13.size() < ASN1_RRC_MAX_SIB) {
        for (uint32_t j = 0; j < data->sched_info_list_r13[i].sib_map_info_r13.size(); j++) {
          uint32_t sib_index = root[i]["si_mapping_info_r13"][j];
          if (sib_index >= 3 && sib_index <= 13) {
            data->sched_info_list_r13[i].sib_map_info_r13[j].value = (sib_type_nb_r13_e::options)(sib_index - 3);
          } else {
            fprintf(stderr, "Invalid SIB index %d for si_mapping_info_r13=%d in sched_info=%d\n", sib_index, j, i);
            return -1;
          }
        }
      } else {
        fprintf(stderr, "Number of si_mapping_info_r13 values exceeds maximum (%d)\n", ASN1_RRC_MAX_SIB);
        return -1;
      }
    } else {
      data->sched_info_list_r13[i].sib_map_info_r13.resize(0);
    }
    if (not parse_enum_by_number(data->sched_info_list_r13[i].si_tb_r13, "si_tb_r13", root[i])) {
      fprintf(stderr, "Missing field si_tb_r13 in sched_info=%d\n", i);
      return -1;
    }
  }
  return 0;
}

int field_nprach_params::parse(libconfig::Setting& root)
{
  data->nprach_params_list_r13.resize((uint32_t)root.getLength());

  printf("NPRACH_PARAMS size %d\n", data->nprach_params_list_r13.size());

  for (uint32_t i = 0; i < data->nprach_params_list_r13.size(); i++) {
    nprach_params_nb_r13_s *param_item = &data->nprach_params_list_r13[i];

    parse_enum_by_number(param_item->nprach_periodicity_r13, "nprach_periodicity_r13", root[i]);
    parse_enum_by_number(param_item->nprach_start_time_r13, "nprach_start_time_r13", root[i]);
    parse_enum_by_number(param_item->nprach_subcarrier_offset_r13, "nprach_subcarrier_offset_r13", root[i]);
    parse_enum_by_number(param_item->nprach_num_subcarriers_r13, "nprach_numsubcarrier_r13", root[i]);
    parse_enum_by_str(param_item->nprach_subcarrier_msg3_range_start_r13, "nprach_subc_msg3_rangestart_r13", root[i]);
    parse_enum_by_number(param_item->max_num_preamb_attempt_ce_r13, "max_num_preamble_attempt_ce_r13", root[i]);
    parse_enum_by_number(param_item->num_repeats_per_preamb_attempt_r13, "num_rep_per_preamble_attempt_r13", root[i]);
    parse_enum_by_number(param_item->npdcch_num_repeats_ra_r13, "npdcch_numrepetition_ra_r13", root[i]);
    parse_enum_by_number_str(param_item->npdcch_start_sf_css_ra_r13, "npdcch_startsf_css_ra_r13", root[i]);
    parse_enum_by_str(param_item->npdcch_offset_ra_r13, "npdcch_offset_ra_r13", root[i]);
  }
  return 0;
}

int field_rach_info::parse(libconfig::Setting& root)
{
  data->rach_info_list_r13.resize((uint32_t)root.getLength());

  for (uint32_t i = 0; i < data->rach_info_list_r13.size(); i++) {
    rach_info_nb_r13_s *rach_info_item = &data->rach_info_list_r13[i];

    parse_enum_by_number(rach_info_item->ra_resp_win_size_r13, "ra_resp_win_size_r13", root[i]);
    parse_enum_by_number(rach_info_item->mac_contention_resolution_timer_r13, "mac_con_res_timer_r13", root[i]);
  }
  return 0;
}

int field_ack_nack_nrep::parse(libconfig::Setting& root)
{
  uint32_t len = root.getLength();
  if (len == 0 || len > 3) {
    fprintf(stderr, "Invalid list size of ack_nack_numrepetitions_nb_r13 %d\n", len);
    return -1;
  }

  data->ack_nack_num_repeats_msg4_r13.resize(len);

  for (uint32_t i = 0; i < len; i++) {
    uint32_t val = root[i];
    asn1::rrc::npusch_cfg_common_nb_r13_s::srs_sf_cfg_r13_opts::options result;
    bool found = asn1::number_to_enum(data->ack_nack_num_repeats_msg4_r13[i], val);
    if (not found) {
      fprintf(stderr, "Invalid value for ack_nack_numrepetitions_nb_r13: %d\n", val);
      return -1;
    }
  }
  return 0;
}

namespace rr_sections {


void fill_cell_common(all_args_t* args_, rrc_cfg_t* rrc_cfg_)
{
  srslte_nbiot_cell_t* cell      = &rrc_cfg_->cell_common;
  srslte_cell_t*       base_cell = &cell->base;

  base_cell->nof_prb = SRSLTE_NBIOT_DEFAULT_NUM_PRB_BASECELL;
  base_cell->nof_ports = args_->enb.nof_ports;
  // to fill cell id
  base_cell->cp = SRSLTE_CP_NORM;

  cell->nbiot_prb = SRSLTE_NBIOT_DEFAULT_PRB_OFFSET;
  // cell->n_id_ncell = 99;
  cell->nof_ports = args_->enb.nof_ports;
  cell->is_r14 = true;
  // cell->mode = SRSLTE_NBIOT_MODE_GUARDBAND; => derived

  rrc_cfg_->cell_spec.rf_port = 0;
}

int parse_rr(all_args_t* args_, rrc_cfg_t* rrc_cfg_)
{
  /* MAC config section */
  parser::section mac_cnfg("mac_cnfg");

  parser::section ulsch_cnfg("ulsch_cnfg_r13");
  mac_cnfg.add_subsection(&ulsch_cnfg);

  ulsch_cnfg.set_optional(&rrc_cfg_->mac_cnfg.ul_sch_cfg_r13_present);
  ulsch_cnfg.add_field(
    make_asn1_enum_number_parser("periodic_bsr_timer_r13",
                                 &rrc_cfg_->mac_cnfg.ul_sch_cfg_r13.periodic_bsr_timer_r13,
                                 &rrc_cfg_->mac_cnfg.ul_sch_cfg_r13.periodic_bsr_timer_r13_present));
  ulsch_cnfg.add_field(
    make_asn1_enum_number_parser("retx_bsr_timer_r13",
                                 &rrc_cfg_->mac_cnfg.ul_sch_cfg_r13.retx_bsr_timer_r13));

  mac_cnfg.add_field(make_asn1_enum_number_parser(
    "time_alignment_timer_r13", &rrc_cfg_->mac_cnfg.time_align_timer_ded_r13));

  rrc_cfg_->mac_cnfg.lc_ch_sr_cfg_r13_present = true;
  rrc_cfg_->mac_cnfg.lc_ch_sr_cfg_r13.set_setup();
  mac_cnfg.add_field(make_asn1_enum_number_parser(
    "logical_channel_sr_prohibit_timer_r13",
    &rrc_cfg_->mac_cnfg.lc_ch_sr_cfg_r13.setup().lc_ch_sr_prohibit_timer_r13));

  /* PHY config section */
  parser::section phy_cnfg("phy_cnfg");

  parser::section npdcch_cnfg_ded("npdcch_cnfg_ded");
  phy_cnfg.add_subsection(&npdcch_cnfg_ded);

  npdcch_cnfg_ded.add_field(make_asn1_enum_number_parser(
    "npdcch_numrepetition_r13",
    &rrc_cfg_->npdcch_cfg.npdcch_num_repeats_r13));
  npdcch_cnfg_ded.add_field(make_asn1_enum_number_str_parser(
    "npdcch_start_sf_uss_r13",
    &rrc_cfg_->npdcch_cfg.npdcch_start_sf_uss_r13));
  npdcch_cnfg_ded.add_field(make_asn1_enum_str_parser(
    "npdcch_offset_uss_r13",
    &rrc_cfg_->npdcch_cfg.npdcch_offset_uss_r13));

  parser::section npusch_cnfg_ded("npusch_cnfg_ded");
  phy_cnfg.add_subsection(&npusch_cnfg_ded);

  npusch_cnfg_ded.add_field(make_asn1_enum_number_parser(
    "ack_nack_numrepetition_r13",
    &rrc_cfg_->npusch_cfg.ack_nack_num_repeats_r13,
    &rrc_cfg_->npusch_cfg.ack_nack_num_repeats_r13_present));
  npusch_cnfg_ded.add_field(new parser::field<bool>(
    "npusch_all_symbols_r13", &rrc_cfg_->npusch_cfg.npusch_all_symbols_r13,
    &rrc_cfg_->npusch_cfg.npusch_all_symbols_r13_present));
  // TODO: Group hop disable?

  // parser::section ul_power_control_ded("ul_power_control_ded");
  // phy_cnfg.add_subsection(&ul_power_control_ded);

  // ul_power_control_ded.add_field(new parser::field<uint8>(
  //   "p0_ue_npusch_r13", &rrc_cfg->phy_cnfg.ul_pwr_ctrl_ded_r13.p0_ue_npusch_r13));

  fill_cell_common(args_, rrc_cfg_);

  parser::section cell_cnfg("cell");

  cell_cnfg.add_field(make_asn1_enum_str_parser("mode", &rrc_cfg_->cell_spec.mode));
  cell_cnfg.add_field(new parser::field<uint32>("cell_id", &rrc_cfg_->cell_spec.cell_id));
  cell_cnfg.add_field(new parser::field<uint16>("tac", &rrc_cfg_->cell_spec.tac));
  cell_cnfg.add_field(new parser::field<uint32>("pci", &rrc_cfg_->cell_spec.pci));
  cell_cnfg.add_field(new parser::field<uint32>("dl_earfcn", &rrc_cfg_->cell_spec.dl_earfcn));
  cell_cnfg.add_field(make_asn1_enum_number_str_parser("dl_raster_offset", &rrc_cfg_->cell_spec.dl_offset));
  cell_cnfg.add_field(new parser::field<uint32>("ul_earfcn", &rrc_cfg_->cell_spec.ul_earfcn));
  cell_cnfg.add_field(make_asn1_enum_number_str_parser("ul_carrier_freq_offset", &rrc_cfg_->cell_spec.ul_carrier_offset));

  // Run parser with two sections
  parser p(args_->enb_files.rr_config);
  p.add_section(&mac_cnfg);
  p.add_section(&phy_cnfg);
  p.add_section(&cell_cnfg);

  return p.parse();
}


} // namespace rr_sections

namespace enb_conf_sections {

int parse_cfg_files(all_args_t* args_, rrc_cfg_t* rrc_cfg_, phy_cfg_t* phy_cfg_)
{
  try {
    if (sib_sections::parse_sibs(args_, rrc_cfg_, phy_cfg_) != SRSLTE_SUCCESS) {
      fprintf(stderr, "Error parsing SIB configuration\n");
      return SRSLTE_ERROR;
    }
  } catch (const SettingTypeException& stex) {
    fprintf(stderr, "Error parsing SIB configuration: %s\n", stex.getPath());
    return SRSLTE_ERROR;
  } catch (const ConfigException& cex) {
    fprintf(stderr, "Error parsing SIB configurationn\n");
    return SRSLTE_ERROR;
  }

  try {
    if (rr_sections::parse_rr(args_, rrc_cfg_) != SRSLTE_SUCCESS) {
      fprintf(stderr, "Error parsing Radio Resources configuration\n");
      return SRSLTE_ERROR;
    }
  } catch (const SettingTypeException& stex) {
    fprintf(stderr, "Error parsing Radio Resources configuration: %s\n", stex.getPath());
    return SRSLTE_ERROR;
  } catch (const ConfigException& cex) {
    fprintf(stderr, "Error parsing Radio Resources configuration\n");
    return SRSLTE_ERROR;
  }

  return enb_conf_sections::set_derived_args(args_, rrc_cfg_, phy_cfg_);
}

static void fill_phy_cell_cfg(all_args_t* args_, rrc_cfg_t* rrc_cfg_, phy_cell_cfg_nb_t* phy_cell_cfg)
{
  // TODO: Currently use a hardwired config before the parser is ready
  srslte_nbiot_cell_t* cell      = &phy_cell_cfg->cell;
  srslte_cell_t*       base_cell = &cell->base;

  bzero(phy_cell_cfg, sizeof(phy_cell_cfg_nb_t));

  // First, set base fields parsed from RRC
  phy_cell_cfg->cell = rrc_cfg_->cell_common;

  // Then, fill the rest
  cell->n_id_ncell = rrc_cfg_->cell_spec.pci;
  switch (rrc_cfg_->cell_spec.mode) {
  case asn1::rrc::mib_nb_s::operation_mode_info_r13_c_::types::standalone_r13:
    cell->mode = SRSLTE_NBIOT_MODE_STANDALONE;
    break;
  case asn1::rrc::mib_nb_s::operation_mode_info_r13_c_::types::guardband_r13:
    cell->mode = SRSLTE_NBIOT_MODE_GUARDBAND;
    break;
  default:
    fprintf(stderr, "Unsupported cell mode %s, using standalone instead\n",
            rrc_cfg_->cell_spec.mode.to_string().c_str());
    cell->mode = SRSLTE_NBIOT_MODE_STANDALONE;
    break;
  }

  phy_cell_cfg->rf_port = 0;
  phy_cell_cfg->cell_id = rrc_cfg_->cell_spec.cell_id;

  // Set UL EARFCN according to DL if not present
  if (rrc_cfg_->cell_spec.ul_earfcn == 0) {
    rrc_cfg_->cell_spec.ul_earfcn = srslte_band_ul_earfcn(rrc_cfg_->cell_spec.dl_earfcn);
  }

  // DL frequency
  phy_cell_cfg->dl_freq_hz = 1e6 * srslte_band_fd(rrc_cfg_->cell_spec.dl_earfcn);
  if (cell->mode != SRSLTE_NBIOT_MODE_STANDALONE) {
    phy_cell_cfg->dl_freq_hz += 1e3 * rrc_cfg_->cell_spec.dl_offset.to_number();
  }

  phy_cell_cfg->ul_freq_hz = 1e6 * srslte_band_fu(rrc_cfg_->cell_spec.ul_earfcn);
  // For non-standalone modes, apply offset according to 5.7.3 in 36.104
  if (cell->mode != SRSLTE_NBIOT_MODE_STANDALONE) {
    phy_cell_cfg->ul_freq_hz += 5e3 * rrc_cfg_->cell_spec.ul_carrier_offset.to_number();
  }

  base_cell->id = rrc_cfg_->cell_spec.cell_id;
}

static void fill_sib_from_cell_cfg(rrc_cfg_t* rrc_cfg_)
{
  sib_type1_nb_s* sib1 = &rrc_cfg_->sib1;
  sib_type2_nb_r13_s* sib2 = &rrc_cfg_->sib2;

  sib1->cell_access_related_info_r13.tac_r13.from_number(rrc_cfg_->cell_spec.tac);
  sib1->cell_access_related_info_r13.cell_id_r13.from_number(rrc_cfg_->cell_spec.cell_id);

  sib1->freq_band_ind_r13 = srslte_band_get_band(rrc_cfg_->cell_spec.dl_earfcn);

  sib2->freq_info_r13.ul_carrier_freq_r13.carrier_freq_r13 = rrc_cfg_->cell_spec.ul_earfcn;
  sib2->freq_info_r13.ul_carrier_freq_r13.carrier_freq_offset_r13_present = true;
  sib2->freq_info_r13.ul_carrier_freq_r13.carrier_freq_offset_r13 = rrc_cfg_->cell_spec.ul_carrier_offset;
}

int set_derived_args(all_args_t* args_, rrc_cfg_t* rrc_cfg_, phy_cfg_t* phy_cfg_)
{
  // TODO: Check for a forced  DL EARFCN or frequency

  // TODO: set config for RRC's base cell

  // Set S1AP related params from cell list
  args_->stack.s1ap.enb_id = args_->enb.enb_id;
  args_->stack.s1ap.cell_id = rrc_cfg_->cell_spec.cell_id;
  args_->stack.s1ap.tac = rrc_cfg_->cell_spec.tac;

  // Create dedicated cell configuration from RRC configuration
  fill_phy_cell_cfg(args_, rrc_cfg_, &phy_cfg_->phy_cell_cfg);

  // Fill certain SIB configurations from cell config
  fill_sib_from_cell_cfg(rrc_cfg_);

  // Patch certain args that are not exposed yet
  args_->rf.nof_carriers = 1;
  args_->rf.nof_antennas = args_->enb.nof_ports;

  // RRC needs eNB id for SIB1 packing
  rrc_cfg_->enb_id = args_->stack.s1ap.enb_id;

  return SRSLTE_SUCCESS;
}

} // namespace enb_conf_sections

namespace sib_sections {

int parse_sib1(std::string filename, sib_type1_nb_s* data)
{
  parser::section sib1("sib1");

  sib1.add_field(make_asn1_enum_str_parser("intra_freq_reselection_r13", &data->cell_access_related_info_r13.intra_freq_resel_r13));
  sib1.add_field(new parser::field<int8>("q_rx_lev_min_r13", &data->cell_sel_info_r13.q_rx_lev_min_r13));
  sib1.add_field(make_asn1_enum_str_parser("cell_barred_r13", &data->cell_access_related_info_r13.cell_barred_r13));
  sib1.add_field(make_asn1_enum_number_parser("si_window_length_r13", &data->si_win_len_r13));

  // sched_info subsection uses a custom field class
  parser::section sched_info("sched_info");
  sib1.add_subsection(&sched_info);
  sched_info.add_field(new field_sched_info(data));

  // Run parser with single section
  return parser::parse_section(std::move(filename), &sib1);
}

int parse_sib2(std::string filename, sib_type2_nb_r13_s* data)
{
  parser::section sib2("sib2");

  sib2.add_field(make_asn1_enum_str_parser("time_alignment_timer_r13", &data->time_align_timer_common_r13));
  sib2.add_field(new parser::field<bool>("cqi_reporting_r14", &data->cqi_report_r14_present));

  parser::section freqinfo("freqInfo_r13");
  sib2.add_subsection(&freqinfo);
  freqinfo.add_field(new parser::field<uint8>("additional_spectrum_emission_r13", &data->freq_info_r13.add_spec_emission_r13));
  freqinfo.add_field(new parser::field<bool>("ul_carrier_freq_r13_present", &data->freq_info_r13.ul_carrier_freq_r13_present));

  // UE timers and constants
  parser::section uetimers("ue_timers_and_constants_r13");
  sib2.add_subsection(&uetimers);
  uetimers.add_field(make_asn1_enum_number_parser("t300_r13", &data->ue_timers_and_consts_r13.t300_r13));
  uetimers.add_field(make_asn1_enum_number_parser("t301_r13", &data->ue_timers_and_consts_r13.t301_r13));
  uetimers.add_field(make_asn1_enum_number_parser("t310_r13", &data->ue_timers_and_consts_r13.t310_r13));
  uetimers.add_field(make_asn1_enum_number_parser("n310_r13", &data->ue_timers_and_consts_r13.n310_r13));
  uetimers.add_field(make_asn1_enum_number_parser("t311_r13", &data->ue_timers_and_consts_r13.t311_r13));
  uetimers.add_field(make_asn1_enum_number_parser("n311_r13", &data->ue_timers_and_consts_r13.n311_r13));

  // Radio-resource configuration section
  parser::section rr_config("rr_config_common_sib_r13");
  sib2.add_subsection(&rr_config);
  rr_cfg_common_sib_nb_r13_s* rr_cfg_common = &data->rr_cfg_common_r13;

  // RACH configuration
  parser::section rach_cnfg("rach_cnfg_r13");
  rr_config.add_subsection(&rach_cnfg);
  rach_cnfg.add_field(make_asn1_enum_number_parser(
      "preamble_init_rx_target_pwr",
      &rr_cfg_common->rach_cfg_common_r13.pwr_ramp_params_r13.preamb_init_rx_target_pwr));
  rach_cnfg.add_field(
      make_asn1_enum_number_parser("pwr_ramping_step",
      &rr_cfg_common->rach_cfg_common_r13.pwr_ramp_params_r13.pwr_ramp_step));
  rach_cnfg.add_field(make_asn1_enum_number_parser(
      "preamble_trans_max_ce_r13", &rr_cfg_common->rach_cfg_common_r13.preamb_trans_max_ce_r13));
  rach_cnfg.add_field(new parser::field<uint8>("conn_est_fail_offset_r13",
      &rr_cfg_common->rach_cfg_common_r13.conn_est_fail_offset_r13,
      &rr_cfg_common->rach_cfg_common_r13.conn_est_fail_offset_r13_present));

  parser::section rach_info("rach_info");
  rach_cnfg.add_subsection(&rach_info);
  rach_info.add_field(new field_rach_info(&rr_cfg_common->rach_cfg_common_r13));

  // BCCH configuration
  parser::section bcch_cnfg("bcch_cnfg_r13");
  rr_config.add_subsection(&bcch_cnfg);
  bcch_cnfg.add_field(
      make_asn1_enum_number_parser("modification_period_coeff_r13", &rr_cfg_common->bcch_cfg_r13.mod_period_coeff_r13));

  // PCCH configuration
  parser::section pcch_cnfg("pcch_cnfg_r13");
  rr_config.add_subsection(&pcch_cnfg);
  pcch_cnfg.add_field(
      make_asn1_enum_number_parser("default_paging_cycle_r13", &rr_cfg_common->pcch_cfg_r13.default_paging_cycle_r13));
  pcch_cnfg.add_field(make_asn1_enum_number_str_parser("nB_r13", &rr_cfg_common->pcch_cfg_r13.nb_r13));
  pcch_cnfg.add_field(make_asn1_enum_number_parser(
      "npdcch_numrepetition_paging_r13",
      &rr_cfg_common->pcch_cfg_r13.npdcch_num_repeat_paging_r13));

  // NPRACH configuration
  parser::section nprach_cnfg("nprach_cnfg_r13");
  rr_config.add_subsection(&nprach_cnfg);
  nprach_cnfg.add_field(make_asn1_enum_number_str_parser(
      "nprach_cp_length_r13", &rr_cfg_common->nprach_cfg_r13.nprach_cp_len_r13));

  parser::section nprach_params("nprach_params_list_r13");
  nprach_cnfg.add_subsection(&nprach_params);
  nprach_params.add_field(new field_nprach_params(&rr_cfg_common->nprach_cfg_r13));

  // NPDSCH configuration
  parser::section npdsch_cnfg("npdsch_cnfg_r13");
  rr_config.add_subsection(&npdsch_cnfg);
  npdsch_cnfg.add_field(new parser::field<int8_t>("nrs_power_r13", &rr_cfg_common->npdsch_cfg_common_r13.nrs_pwr_r13));

  // NPUSCH configuration: TODO ACK/NACK re
  parser::section npusch_cnfg("npusch_cnfg_r13");
  rr_config.add_subsection(&npusch_cnfg);

  parser::section ack_nack_nrep("ack_nack_numrepetitions_nb_r13");
  npusch_cnfg.add_subsection(&ack_nack_nrep);
  ack_nack_nrep.add_field(new field_ack_nack_nrep(&rr_cfg_common->npusch_cfg_common_r13));

  // NPUSCH-ULRS configuration
  parser::section ulrs_cnfg("ul_rs_r13");
  npusch_cnfg.add_subsection(&ulrs_cnfg);
  ulrs_cnfg.add_field(new parser::field<uint8>(
      "group_assignment_npusch_r13", &rr_cfg_common->npusch_cfg_common_r13.ul_ref_sigs_npusch_r13.group_assign_npusch_r13));
  ulrs_cnfg.add_field(new parser::field<bool>(
      "group_hopping_enabled_r13", &rr_cfg_common->npusch_cfg_common_r13.ul_ref_sigs_npusch_r13.group_hop_enabled_r13));

  // DL Gap configuration
  parser::section dl_gap("dl_gap_r13");
  rr_config.add_subsection(&dl_gap);
  dl_gap.set_optional(&rr_cfg_common->dl_gap_r13_present);
  dl_gap.add_field(make_asn1_enum_number_parser("dl_gap_thres_r13", &rr_cfg_common->dl_gap_r13.dl_gap_thres_r13));
  dl_gap.add_field(make_asn1_enum_number_parser("dl_gap_periodicity_r13", &rr_cfg_common->dl_gap_r13.dl_gap_periodicity_r13));
  dl_gap.add_field(make_asn1_enum_str_parser("dl_gap_duration_coeff_r13", &rr_cfg_common->dl_gap_r13.dl_gap_dur_coeff_r13));

  // UL PWR Ctrl configuration
  parser::section ul_pwr_ctrl("ul_pwr_ctrl_r13");
  rr_config.add_subsection(&ul_pwr_ctrl);
  ul_pwr_ctrl.add_field(
      new parser::field<int8>("p0_nominal_npusch_r13", &rr_cfg_common->ul_pwr_ctrl_common_r13.p0_nominal_npusch_r13));
  ul_pwr_ctrl.add_field(make_asn1_enum_str_parser("alpha_r13", &rr_cfg_common->ul_pwr_ctrl_common_r13.alpha_r13));
  ul_pwr_ctrl.add_field(
      new parser::field<int8>("delta_preamble_msg3_r13", &rr_cfg_common->ul_pwr_ctrl_common_r13.delta_preamb_msg3_r13));


  // Run parser with single section
  return parser::parse_section(std::move(filename), &sib2);
}

int parse_sib3(std::string filename, sib_type3_nb_r13_s* data)
{
  parser::section sib3("sib3");

  // CellReselectionInfoCommon
  parser::section resel_common("cell_reselection_common_r13");
  sib3.add_subsection(&resel_common);

  resel_common.add_field(make_asn1_enum_number_parser("q_hyst_r13", &data->cell_resel_info_common_r13.q_hyst_r13));

  // CellReselectionServingFreqInfo
  parser::section resel_serving("cell_reselection_serving_r13");
  sib3.add_subsection(&resel_serving);

  resel_serving.add_field(new parser::field<uint8>(
      "s_non_intra_search_r13", &data->cell_resel_serving_freq_info_r13.s_non_intra_search_r13));

  // intraFreqCellReselectionInfo
  parser::section intra_freq("intra_freq_reselection_r13");
  sib3.add_subsection(&intra_freq);
  sib_type3_nb_r13_s::intra_freq_cell_resel_info_r13_s_* intrafreq = &data->intra_freq_cell_resel_info_r13;

  intra_freq.add_field(new parser::field<int8_t>("q_rx_lev_min_r13", &intrafreq->q_rx_lev_min_r13));
  intra_freq.add_field(new parser::field<int8>("p_max_r13", &intrafreq->p_max_r13, &intrafreq->p_max_r13_present));
  intra_freq.add_field(new parser::field<int8>(
      "q_qual_min_r13", &intrafreq->q_qual_min_r13, &intrafreq->q_qual_min_r13_present));
  intra_freq.add_field(
      new parser::field<uint8>("s_intra_search_p_r13", &intrafreq->s_intra_search_p_r13));
  intra_freq.add_field(make_asn1_enum_number_parser("t_resel_r13", &intrafreq->t_resel_r13));

  // Run parser with single section
  return parser::parse_section(std::move(filename), &sib3);
}

int parse_sibs(all_args_t* args_, rrc_cfg_t* rrc_cfg_, phy_cfg_t* phy_config_common)
{
  sib_type1_nb_s* sib1 = &rrc_cfg_->sib1;
  sib_type2_nb_r13_s* sib2 = &rrc_cfg_->sib2;
  sib_type3_nb_r13_s* sib3 = &rrc_cfg_->sib3;

  if (sib_sections::parse_sib1(args_->enb_files.sib_config, sib1) != SRSLTE_SUCCESS) {
    return SRSLTE_ERROR;
  }

  // Fill rest of data from enb config
  std::string mnc_str;
  if (not srslte::mnc_to_string(args_->stack.s1ap.mnc, &mnc_str)) {
    ERROR("The provided mnc=%d is not valid", args_->stack.s1ap.mnc);
    return -1;
  }
  std::string mcc_str;
  if (not srslte::mcc_to_string(args_->stack.s1ap.mcc, &mcc_str)) {
    ERROR("The provided mnc=%d is not valid", args_->stack.s1ap.mcc);
    return -1;
  }
  sib_type1_nb_s::cell_access_related_info_r13_s_* cell_access = &sib1->cell_access_related_info_r13;
  cell_access->plmn_id_list_r13.resize(1);
  srslte::plmn_id_t plmn;
  if (plmn.from_string(mcc_str + mnc_str) == SRSLTE_ERROR) {
    ERROR("Could not convert %s to a plmn_id\n", (mcc_str + mnc_str).c_str());
    return -1;
  }
  srslte::to_asn1(&cell_access->plmn_id_list_r13[0].plmn_id_r13, plmn);
  cell_access->plmn_id_list_r13[0].cell_reserved_for_oper_r13 =
      plmn_id_info_nb_r13_s::cell_reserved_for_oper_r13_e_::not_reserved;

  // Will be updated in MAC, just set to 0 here
  sib1->hyper_sfn_msb_r13.from_number(0);

  // Generate SIB2
  if (sib_sections::parse_sib2(args_->enb_files.sib_config, sib2) != SRSLTE_SUCCESS) {
    return SRSLTE_ERROR;
  }

  if (sib_sections::parse_sib3(args_->enb_files.sib_config, sib3)) {
    ERROR("SIB3 parsing error\n");
    return SRSLTE_ERROR;
  }

  phy_config_common->nprach_cnfg = sib2->rr_cfg_common_r13.nprach_cfg_r13;
  phy_config_common->npusch_cnfg = sib2->rr_cfg_common_r13.npusch_cfg_common_r13;

  return SRSLTE_SUCCESS;
}

} // namespace sib_sections

} // namespace sonica_enb