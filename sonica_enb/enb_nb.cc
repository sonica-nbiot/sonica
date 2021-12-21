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

#include "sonica_enb/hdr/enb_nb.h"
#include "sonica_enb/hdr/stack/enb_stack_nb.h"
#include "enb_cfg_parser.h"
#include "sonica/build_info.h"
#include <iostream>

namespace sonica_enb {

enb_nb::enb_nb() : started(false), pool(srslte::byte_buffer_pool::get_instance(ENB_POOL_SIZE))
{
  // print build info
  std::cout << std::endl << get_build_string() << std::endl;
}

enb_nb::~enb_nb()
{
  // pool has to be cleaned after enb is deleted
  stack.reset();
  srslte::byte_buffer_pool::cleanup();
}

int enb_nb::init(const all_args_t& args_, srslte::logger* logger_)
{
  int ret = SRSLTE_SUCCESS;
  logger = logger_;

  // Init UE log
  log.init("ENB ", logger);
  log.set_level(srslte::LOG_LEVEL_INFO);
  log.info("%s", get_build_string().c_str());


  // Validate arguments
  if (parse_args(args_)) {
    log.console("Error processing arguments.\n");
    return SRSLTE_ERROR;
  }

  pool_log.init("POOL", logger);
  pool_log.set_level(srslte::LOG_LEVEL_ERROR);
  pool->set_log(&pool_log);

  std::unique_ptr<enb_stack_nb> nb_stack(new enb_stack_nb(logger));
  if (!nb_stack) {
    log.console("Error creating eNB stack.\n");
    return SRSLTE_ERROR;
  }

  std::unique_ptr<srslte::radio> lte_radio = std::unique_ptr<srslte::radio>(new srslte::radio(logger));
  if (!lte_radio) {
    log.console("Error creating radio multi instance.\n");
    return SRSLTE_ERROR;
  }

  std::unique_ptr<sonica_enb::phy> nb_phy = std::unique_ptr<sonica_enb::phy>(new sonica_enb::phy(logger));
  if (!nb_phy) {
    log.console("Error creating LTE PHY instance.\n");
    return SRSLTE_ERROR;
  }

  // Init layers
  if (lte_radio->init(args.rf, nullptr)) {
    log.console("Error initializing radio.\n");
    ret = SRSLTE_ERROR;
  }

  if (nb_phy->init(args.phy, phy_cfg, lte_radio.get(), nb_stack.get())) {
    log.console("Error initializing stack.\n");
    ret = SRSLTE_ERROR;
  }

  if (nb_stack->init(args.stack, rrc_cfg, /* nb_phy.get() */ nullptr)) {
    log.console("Error initializing stack.\n");
    ret = SRSLTE_ERROR;
  }

  // TODO: Initialize radio, phy and stack
  stack = std::move(nb_stack);
  phy_h = std::move(nb_phy);
  radio = std::move(lte_radio);

  log.console("\n==== eNodeB started ===\n");
  log.console("Type <t> to view trace\n");

  started = (ret == SRSLTE_SUCCESS);

  return ret;
}

void enb_nb::stop()
{
  if (started) {
  	// TODO
    if (phy_h) {
      phy_h->stop();
    }

    if (stack) {
      stack->stop();
    }

    if (radio) {
      radio->stop();
    }

    started = false;
  }
}

int enb_nb::parse_args(const all_args_t& args_)
{
  // set member variable
  args = args_;

  return enb_conf_sections::parse_cfg_files(&args, &rrc_cfg, &phy_cfg);
}

void enb_nb::print_pool()
{
  srslte::byte_buffer_pool::get_instance()->print_all_buffers();
}

// bool enb_nb::get_metrics(srsenb::enb_metrics_t* m)
// {
  // TODO
  // radio->get_metrics(&m->rf);
  // phy->get_metrics(m->phy);
  // stack->get_metrics(&m->stack);
  // m->running = started;
//   return true;
// }

srslte::LOG_LEVEL_ENUM enb_nb::level(std::string l)
{
  std::transform(l.begin(), l.end(), l.begin(), ::toupper);
  if ("NONE" == l) {
    return srslte::LOG_LEVEL_NONE;
  } else if ("ERROR" == l) {
    return srslte::LOG_LEVEL_ERROR;
  } else if ("WARNING" == l) {
    return srslte::LOG_LEVEL_WARNING;
  } else if ("INFO" == l) {
    return srslte::LOG_LEVEL_INFO;
  } else if ("DEBUG" == l) {
    return srslte::LOG_LEVEL_DEBUG;
  } else {
    return srslte::LOG_LEVEL_NONE;
  }
}

std::string enb_nb::get_build_mode()
{
  return std::string(sonica_get_build_mode());
}

std::string enb_nb::get_build_info()
{
  if (std::string(sonica_get_build_info()).find("  ") != std::string::npos) {
    return std::string(sonica_get_version());
  }
  return std::string(sonica_get_build_info());
}

std::string enb_nb::get_build_string()
{
  std::stringstream ss;
  ss << "Built in " << get_build_mode() << " mode using " << get_build_info() << "." << std::endl;
  return ss.str();
}

} // namespace sonica_enb