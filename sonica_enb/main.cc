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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "CLI11.hpp"

#include "srslte/common/config_file.h"
#include "srslte/common/crash_handler.h"
#include "srslte/common/signal_handler.h"

#include <iostream>
#include <memory>
#include <string>

#include "sonica_enb/hdr/enb_nb.h"

using namespace std;
using namespace sonica_enb;

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/
string config_file;

static std::string get_version_string()
{
  ostringstream os;

  os << srslte_get_version_major() << "." << srslte_get_version_minor() << "."
         << srslte_get_version_patch();

  return os.str();
}

int parse_args(all_args_t* args, int argc, char* argv[])
{
  CLI::App app{"Sonica eNodeB"};

  string mcc;
  string mnc;
  string enb_id;

  app.set_version_flag("--version,-v", get_version_string, "Print version information and exit");
  app.set_help_all_flag("--help-all", "Expand all help");

  CLI::App *general = app.add_subcommand("general")->configurable();
  general->add_option("--enb_id", enb_id, "eNodeB ID")->default_val("0x0");
  general->add_option("--name", args->stack.s1ap.enb_name, "eNodeB Name")->default_val("srsenb01");
  general->add_option("--mcc", mcc, "Mobile Country Code")->default_val("001");
  general->add_option("--mnc", mnc, "Mobile Network Code")->default_val("01");
  general->add_option("--mme_addr", args->stack.s1ap.mme_addr, "IP address of MME for S1 connection")->default_val("127.0.0.1");
  general->add_option("--gtp_bind_addr", args->stack.s1ap.gtp_bind_addr, "Local IP address to bind for GTP connection")->default_val("192.168.3.1");
  general->add_option("--s1c_bind_addr", args->stack.s1ap.s1c_bind_addr, "Local IP address to bind for S1AP connection")->default_val("192.168.3.1");
  general->add_option("--nof_ports", args->enb.nof_ports, "Number of ports")->default_val(1);
  general->add_option("--mode", args->enb.cell_mode, "Operating Mode")->default_val("standalone");

  CLI::App *enb_files = app.add_subcommand("enb_files")->configurable();
  enb_files->add_option("--sib_config", args->enb_files.sib_config, "SIB configuration files")->default_val("sib_nb.conf");
  enb_files->add_option("--rr_config", args->enb_files.rr_config, "RR configuration files")->default_val("rr_nb.conf");
  enb_files->add_option("--drb_config", args->enb_files.drb_config, "DRB configuration files")->default_val("drb_nb.conf");

  CLI::App *rf = app.add_subcommand("rf")->configurable();
  rf->add_option("--dl_earfcn", args->enb.dl_earfcn, "Force Downlink EARFCN for single cell")->default_val(0);
  rf->add_option("--rx_gain", args->rf.rx_gain, "Front-end receiver gain")->default_val(50);
  rf->add_option("--tx_gain", args->rf.tx_gain, "Front-end transmitter gain")->default_val(70);
  rf->add_option("--dl_freq", args->rf.dl_freq, "Downlink Frequency (if positive overrides EARFCN)")->default_val(-1);
  rf->add_option("--ul_freq", args->rf.ul_freq, "Uplink Frequency (if positive overrides EARFCN)")->default_val(-1);

  rf->add_option("--device_name", args->rf.device_name, "Front-end device name")->default_val("auto");
  rf->add_option("--device_args", args->rf.device_args, "Front-end device arguments")->default_val("auto");
  rf->add_option("--time_adv_nsamples", args->rf.time_adv_nsamples, "Transmission time advance")->default_val("auto");

  CLI::App *log = app.add_subcommand("log")->configurable();
  log->add_option("--rf_level", args->rf.log_level, "RF log level");
  log->add_option("--phy_level", args->phy.log.phy_level, "PHY log level");
  log->add_option("--phy_hex_limit", args->phy.log.phy_hex_limit, "PHY log hex dump limit");
  log->add_option("--phy_lib_level", args->phy.log.phy_lib_level, "PHY lib log level")->default_val("none");
  log->add_option("--mac_level", args->stack.log.mac_level, "MAC log level");
  log->add_option("--mac_hex_limit", args->stack.log.mac_hex_limit, "MAC log hex dump limit");
  log->add_option("--rlc_level", args->stack.log.rlc_level, "RLC log level");
  log->add_option("--rlc_hex_limit", args->stack.log.rlc_hex_limit, "RLC log hex dump limit");
  log->add_option("--pdcp_level", args->stack.log.pdcp_level, "PDCP log level");
  log->add_option("--pdcp_hex_limit", args->stack.log.pdcp_hex_limit, "PDCP log hex dump limit");
  log->add_option("--rrc_level", args->stack.log.rrc_level, "RRC log level");
  log->add_option("--rrc_hex_limit", args->stack.log.rrc_hex_limit, "RRC log hex dump limit");
  log->add_option("--gtpu_level",    args->stack.log.gtpu_level, "GTPU log level");
  log->add_option("--gtpu_hex_limit", args->stack.log.gtpu_hex_limit, "GTPU log hex dump limit");
  log->add_option("--s1ap_level", args->stack.log.s1ap_level, "S1AP log level");
  log->add_option("--s1ap_hex_limit", args->stack.log.s1ap_hex_limit, "S1AP log hex dump limit");
  log->add_option("--stack_level", args->stack.log.stack_level, "Stack log level");
  log->add_option("--stack_hex_limit", args->stack.log.stack_hex_limit, "Stack log hex dump limit");

  log->add_option("--all_level", args->log.all_level, "ALL log level")->default_val("info");
  log->add_option("--all_hex_limit", args->log.all_hex_limit, "ALL log hex dump limit")->default_val(32);

  log->add_option("--filename", args->log.filename, "Log filename")->default_val("/tmp/enb_nb.log");
  log->add_option("--file_max_size", args->log.file_max_size, "Maximum file size (in kilobytes). When passed, multiple files are created. Default -1 (single file)")->default_val(-1);


  CLI::App *pcap = app.add_subcommand("pcap")->configurable();
  pcap->add_option("--enable", args->stack.mac_pcap.enable, "Enable MAC packet captures for wireshark")->default_val(false);
  pcap->add_option("--filename", args->stack.mac_pcap.filename, "MAC layer capture filename")->default_val("enb_mac.pcap");
  pcap->add_option("--s1ap_enable", args->stack.s1ap_pcap.enable, "Enable S1AP packet captures for wireshark")->default_val(false);
  pcap->add_option("--s1ap_filename", args->stack.s1ap_pcap.filename, "S1AP layer capture filename")->default_val("enb_s1ap.pcap");

  CLI::App *expert = app.add_subcommand("expert")->configurable();
  expert->add_option("--emulate_nprach", args->phy.emulate_nprach, "Enable emulated nprach")->default_val(false);

  app.add_option("config_file", config_file, "eNodeB configuration file");

  // parse the command line and store result in vm
  CLI11_PARSE(app, argc, argv);

  // if no config file given, check users home path
  if (!app.count("config_file")) {
    if (!config_exists(config_file, "enb_nb.conf")) {
      cout << "Failed to read eNB configuration file " << config_file << " - exiting" << endl;
      exit(1);
    }
  }

  cout << "Reading configuration file " << config_file << "..." << endl;
  ifstream conf(config_file.c_str(), ios::in);
  if (conf.fail()) {
    cout << "Failed to read configuration file " << config_file << " - exiting" << endl;
    exit(1);
  }

  // parse config file and handle errors gracefully
  try {
    app.parse_from_stream(conf);
  } catch (const CLI::ParseError &e) {
    cout << "Error from config file: " << e.get_name() << endl;
    return app.exit(e);
  }


  // Convert MCC/MNC strings
  if (!srslte::string_to_mcc(mcc, &args->stack.s1ap.mcc)) {
    cout << "Error parsing enb.mcc:" << mcc << " - must be a 3-digit string." << endl;
  }
  if (!srslte::string_to_mnc(mnc, &args->stack.s1ap.mnc)) {
    cout << "Error parsing enb.mnc:" << mnc << " - must be a 2 or 3-digit string." << endl;
  }

  // Covert eNB Id
  std::size_t pos = {};
  try {
    args->enb.enb_id = std::stoi(enb_id, &pos, 0);
  } catch (...) {
    cout << "Error parsing enb.enb_id: " << enb_id << "." << endl;
    exit(1);
  }
  if (pos != enb_id.size()) {
    cout << "Error parsing enb.enb_id: " << enb_id << "." << endl;
    exit(1);
  }

  // Apply all_level to any unset layers
  if (log->count("--all_level")) {
    if (!log->count("--rf_level")) {
      args->rf.log_level = args->log.all_level;
    }
    if (!log->count("--phy_level")) {
      args->phy.log.phy_level = args->log.all_level;
    }
    if (!log->count("--phy_lib_level")) {
      args->phy.log.phy_lib_level = args->log.all_level;
    }
    if (!log->count("--mac_level")) {
      args->stack.log.mac_level = args->log.all_level;
    }
    if (!log->count("--rlc_level")) {
      args->stack.log.rlc_level = args->log.all_level;
    }
    if (!log->count("--pdcp_level")) {
      args->stack.log.pdcp_level = args->log.all_level;
    }
    if (!log->count("--rrc_level")) {
      args->stack.log.rrc_level = args->log.all_level;
    }
    if (!log->count("--gtpu_level")) {
      args->stack.log.gtpu_level = args->log.all_level;
    }
    if (!log->count("--s1ap_level")) {
      args->stack.log.s1ap_level = args->log.all_level;
    }
    if (!log->count("--stack_level")) {
      args->stack.log.stack_level = args->log.all_level;
    }
  }

  // Apply all_hex_limit to any unset layers
  if (log->count("--all_hex_limit")) {
    if (!log->count("--phy_hex_limit")) {
      args->log.phy_hex_limit = args->log.all_hex_limit;
    }
    if (!log->count("--mac_hex_limit")) {
      args->stack.log.mac_hex_limit = args->log.all_hex_limit;
    }
    if (!log->count("--rlc_hex_limit")) {
      args->stack.log.rlc_hex_limit = args->log.all_hex_limit;
    }
    if (!log->count("--pdcp_hex_limit")) {
      args->stack.log.pdcp_hex_limit = args->log.all_hex_limit;
    }
    if (!log->count("--rrc_hex_limit")) {
      args->stack.log.rrc_hex_limit = args->log.all_hex_limit;
    }
    if (!log->count("--gtpu_hex_limit")) {
      args->stack.log.gtpu_hex_limit = args->log.all_hex_limit;
    }
    if (!log->count("--s1ap_hex_limit")) {
      args->stack.log.s1ap_hex_limit = args->log.all_hex_limit;
    }
    if (!log->count("--stack_hex_limit")) {
      args->stack.log.stack_hex_limit = args->log.all_hex_limit;
    }
  }

  // Check remaining eNB config files
  if (!config_exists(args->enb_files.sib_config, "sib_nb.conf")) {
    cout << "Failed to read SIB configuration file " << args->enb_files.sib_config << " - exiting" << endl;
    exit(1);
  }

  if (!config_exists(args->enb_files.rr_config, "rr_nb.conf")) {
    cout << "Failed to read RR configuration file " << args->enb_files.rr_config << " - exiting" << endl;
    exit(1);
  }

  if (!config_exists(args->enb_files.drb_config, "drb_nb.conf")) {
    cout << "Failed to read DRB configuration file " << args->enb_files.drb_config << " - exiting" << endl;
    exit(1);
  }

  return 1;
}

void* input_loop(void* m)
{
  // metrics_stdout* metrics = (metrics_stdout*)m;
  char            key;
  while (running) {
    cin >> key;
    if (cin.eof() || cin.bad()) {
      cout << "Closing stdin thread." << endl;
      break;
    } else {
      if ('t' == key) {
        // do_metrics = !do_metrics;
        // if (do_metrics) {
        //   cout << "Enter t to stop trace." << endl;
        // } else {
        //   cout << "Enter t to restart trace." << endl;
        // }
        // metrics->toggle_print(do_metrics);
      } else if ('q' == key) {
        raise(SIGTERM);
      }
    }
  }
  return nullptr;
}

int main(int argc, char* argv[])
{
  srslte_register_signal_handler();
  all_args_t                         args = {};
  // srslte::metrics_hub<enb_metrics_t> metricshub;
  // metrics_stdout                     metrics_screen;

  cout << "---  Software Radio Systems LTE eNodeB (NB-IoT version)  ---" << endl << endl;

  srslte_debug_handle_crash(argc, argv);
  if (parse_args(&args, argc, argv) != 1) {
    exit(-1);
  }

  srslte::logger_stdout logger_stdout;

  // Set logger
  srslte::logger* logger = nullptr;
  if (args.log.filename == "stdout") {
    logger = &logger_stdout;
  } else {
    logger_file.init(args.log.filename, args.log.file_max_size);
    logger = &logger_file;
  }
  srslte::logmap::set_default_logger(logger);
  srslte::logmap::get("COMMON")->set_level(srslte::LOG_LEVEL_INFO);

  // Create eNB
  unique_ptr<sonica_enb::enb_nb> enb{new sonica_enb::enb_nb};
  if (enb->init(args, logger) != SRSLTE_SUCCESS) {
    enb->stop();
    return SRSLTE_ERROR;
  }

  // Set metrics
  // metricshub.init(enb.get(), args.general.metrics_period_secs);
  // metricshub.add_listener(&metrics_screen);
  // metrics_screen.set_handle(enb.get());

  // srsenb::metrics_csv metrics_file(args.general.metrics_csv_filename);
  // if (args.general.metrics_csv_enable) {
  //   metricshub.add_listener(&metrics_file);
  //   metrics_file.set_handle(enb.get());
  // }

  // create input thread
  pthread_t input;
  pthread_create(&input, nullptr, &input_loop, NULL /* &metrics_screen */);

  bool signals_pregenerated = false;
  // if (running) {
  //   if (args.gui.enable) {
  //     enb->start_plot();
  //   }
  // }
  int cnt = 0;
  while (running) {
    if (args.general.print_buffer_state) {
      cnt++;
      if (cnt == 1000) {
        cnt = 0;
        // enb->print_pool();
      }
    }
    usleep(10000);
  }
  pthread_cancel(input);
  pthread_join(input, NULL);
  // metricshub.stop();
  enb->stop();
  cout << "---  exiting  ---" << endl;

  return SRSLTE_SUCCESS;
}
