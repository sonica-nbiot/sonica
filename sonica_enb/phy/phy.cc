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
#include <sstream>
#include <string.h>
#include <string>
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sonica_enb/hdr/phy/phy.h"
#include "srslte/common/log.h"
#include "srslte/common/threads.h"

#define Error(fmt, ...)                                                                                                \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...)                                                                                              \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...)                                                                                                 \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)                                                                                                \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->debug(fmt, ##__VA_ARGS__)

using namespace std;
using namespace asn1::rrc;

namespace sonica_enb {

phy::phy(srslte::logger* logger_) :
  logger(logger_), workers_pool(MAX_WORKERS), workers(MAX_WORKERS), workers_common(), nof_workers(0)
{
}

phy::~phy()
{
  stop();
}

int phy::init(const phy_args_t&            args,
              const phy_cfg_t&             cfg,
              srslte::radio_interface_phy* radio_,
              stack_interface_phy_nb*      stack_)
{
  mlockall((uint32_t)MCL_CURRENT | (uint32_t)MCL_FUTURE);

  // Create array of pointers to phy_logs
  for (int i = 0; i < args.nof_phy_threads; i++) {
    auto mylog   = std::unique_ptr<srslte::log_filter>(new srslte::log_filter);
    char tmp[16] = {};
    sprintf(tmp, "PHY%d", i);
    mylog->init(tmp, logger, true);
    mylog->set_level(args.log.phy_level);
    mylog->set_hex_limit(args.log.phy_hex_limit);
    log_vec.push_back(std::move(mylog));
  }
  log_h = log_vec[0].get();

  // Add PHY lib log
  if (log_vec.at(0)->get_level_from_string(args.log.phy_lib_level) != srslte::LOG_LEVEL_NONE) {
    auto lib_log = std::unique_ptr<srslte::log_filter>(new srslte::log_filter);
    char tmp[16] = {};
    sprintf(tmp, "PHY_LIB");
    lib_log->init(tmp, logger, true);
    lib_log->set_level(args.log.phy_lib_level);
    lib_log->set_hex_limit(args.log.phy_hex_limit);
    log_vec.push_back(std::move(lib_log));
  } else {
    log_vec.push_back(nullptr);
  }

  radio       = radio_;
  stack       = stack_;
  nof_workers = 1;//args.nof_phy_threads;

  workers_common.params = args;

  workers_common.init(cfg.phy_cell_cfg, radio, stack_);

  // TODO
  // parse_common_config(cfg);

  // Add workers to workers pool and start threads
  for (uint32_t i = 0; i < nof_workers; i++) {
    printf("INIT W%d\n", i);
    workers[i].init(&workers_common, log_vec.at(i).get());
    workers_pool.init_worker(i, &workers[i], WORKERS_THREAD_PRIO);
  }

  nprach.init(cfg.phy_cell_cfg.cell, args, stack_, log_vec.at(0).get(), PRACH_WORKER_THREAD_PRIO);

  // Warning this must be initialized after all workers have been added to the pool
  tx_rx.init(radio, &workers_pool, &workers_common, &nprach, log_vec.at(0).get(), SF_RECV_THREAD_PRIO);

  initialized = true;

  return SRSLTE_SUCCESS;
}

void phy::stop()
{
  if (initialized) {
    // TODO
    tx_rx.stop();
    workers_common.stop();
    workers_pool.stop();
    nprach.stop();

    initialized = false;
  }
}

} // namespace sonica_enb