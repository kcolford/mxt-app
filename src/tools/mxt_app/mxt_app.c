//------------------------------------------------------------------------------
/// \file   mxt_app.c
/// \brief  Command line tool for Atmel maXTouch chips.
/// \author Nick Dyer
//------------------------------------------------------------------------------
// Copyright 2011 Atmel Corporation. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY ATMEL ''AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL ATMEL OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
// OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//------------------------------------------------------------------------------

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>

#include "libmaxtouch/libmaxtouch.h"
#include "libmaxtouch/i2c_dev/i2c_dev_device.h"
#include "libmaxtouch/log.h"
#include "libmaxtouch/utilfuncs.h"
#include "libmaxtouch/info_block.h"

#include "mxt_app.h"

#define BUF_SIZE 1024

//******************************************************************************
/// \brief Commands for mxt-app
typedef enum mxt_app_cmd_tag {
  CMD_NONE,
  CMD_TEST,
  CMD_WRITE,
  CMD_READ,
  CMD_GOLDEN_REFERENCES,
  CMD_BRIDGE_CLIENT,
  CMD_BRIDGE_SERVER,
  CMD_SERIAL_DATA,
  CMD_FLASH,
  CMD_RESET,
  CMD_RESET_BOOTLOADER,
  CMD_BACKUP,
  CMD_CALIBRATE,
  CMD_DEBUG_DUMP,
  CMD_LOAD_CFG,
  CMD_SAVE_CFG,
} mxt_app_cmd;

//******************************************************************************
/// \brief Initialize mXT device and read the info block
static int mxt_init_chip(int adapter, int address)
{
  int ret;

  if (adapter >= 0 && address > 0)
  {
    LOG(LOG_DEBUG, "i2c_address:%u", address);
    LOG(LOG_DEBUG, "i2c_adapter:%u", adapter);
    ret = i2c_dev_set_address(adapter, address);
    if (ret < 0)
    {
      printf("Failed to init device - exiting the application\n");
      return -1;
    }
  }
  else
  {
    ret = mxt_scan();
    if (ret == 0)
    {
      printf("Unable to find any maXTouch devices - exiting the application\n");
      return -1;
    }
    else if (ret < 0)
    {
      printf("Failed to init device - exiting the application\n");
      return -1;
    }
  }

  if (mxt_get_info() < 0)
  {
    printf("Error reading info block, exiting...\n");
    return -1;
  }

  return 0;
}

//******************************************************************************
/// \brief Print usage for mxt-app
static void print_usage(char *prog_name)
{
  fprintf(stderr, "Command line tool for Atmel maXTouch chips version: %s\n\n"
                  "Usage: %s [command] [args]\n\n"
                  "When run with no options, access menu interface.\n\n"
                  "Available commands:\n"
                  "  -h [--help]                : display this help and exit\n"
                  "  -R [--read]                : read from object\n"
                  "  -t [--test]                : run all self tests\n"
                  "  -W [--write]               : write to object\n"
                  "  --flash FIRMWARE           : send FIRMWARE to bootloader\n"
                  "  --reset                    : reset device\n"
                  "  --reset-bootloader         : reset device in bootloader mode\n"
                  "  --backup                   : backup configuration to NVRAM\n"
                  "  --calibrate                : send calibrate command\n"
                  "  --debug-dump FILE          : capture diagnostic data to FILE\n"
                  "  --load FILE                : upload config from FILE\n"
                  "  --save FILE                : save config to FILE\n"
                  "  -g                         : store golden references\n"
                  "  --version                  : print version\n"
                  "\n"
                  "Valid options:\n"
                  "  -n [--count] COUNT         : read/write COUNT registers\n"
                  "  -f [--format]              : format register output\n"
                  "  -I [--instance] INSTANCE   : select object INSTANCE\n"
                  "  -r [--register] REGISTER   : start at REGISTER\n"
                  "  -T [--type] TYPE           : select object TYPE\n"
                  "  -v [--verbose] LEVEL       : print additional debug\n"
                  "\n"
                  "For TCP socket:\n"
                  "  -C [--bridge-client] HOST  : connect over TCP to HOST\n"
                  "  -S [--bridge-server]       : start TCP socket server\n"
                  "  -p [--port] PORT           : TCP port (default 4000)\n"
                  "\n"
                  "For bootloader mode:\n"
                  "  --firmware-version VERSION : Check firmware VERSION "
                                                 "before and after flash\n"
                  "\n"
                  "For T37 diagnostic data:\n"
                  "  --frames N                 : Capture N frames of data\n"
                  "  --references               : Dump references data\n"
                  "\n"
                  "For i2c-dev and bootloader mode:\n"
                  "  -d [--i2c-adapter] ADAPTER : i2c adapter, eg \"2\"\n"
                  "  -a [--i2c-address] ADDRESS : i2c address, eg \"4a\"\n"
                  "\n"
                  "For T68 serial data:\n"
                  "  --t68-file FILE            : Upload FILE\n"
                  "  --t68-datatype DATATYPE    : Select DATATYPE\n"
                  "\n"
                  "Examples:\n"
                  "  %s -R -n7 -r0      : Read info block\n"
                  "  %s -R -T9 --format : Read T9 object, formatted output\n"
                  "  %s -W -T7 0000     : Zero first two bytes of T7\n"
                  "  %s --test          : run self tests\n",
                  __GIT_VERSION,
                  prog_name, prog_name, prog_name, prog_name, prog_name);
}

//******************************************************************************
/// \brief Main function for mxt-app
int main (int argc, char *argv[])
{
  int ret;
  int c;
  uint16_t address = 0;
  uint16_t object_address = 0;
  uint8_t count = 0;
  int i2c_address = -1;
  int i2c_adapter = -1;
  uint16_t object_type = 0;
  uint8_t instance = 0;
  uint8_t verbose = 0;
  uint16_t t37_frames = 1;
  uint8_t t37_mode = DELTAS_MODE;
  bool format = false;
  uint16_t port = 4000;
  uint8_t t68_datatype = 1;
  unsigned char databuf[BUF_SIZE];
  char strbuf2[BUF_SIZE];
  char strbuf[BUF_SIZE];
  strbuf[0] = '\0';
  strbuf2[0] = '\0';
  mxt_app_cmd cmd = CMD_NONE;

  while (1)
  {
    int option_index = 0;

    static struct option long_options[] = {
      {"i2c-address",      required_argument, 0, 'a'},
      {"backup",           no_argument,       0, 0},
      {"bridge-client",    required_argument, 0, 'C'},
      {"calibrate",        no_argument,       0, 0},
      {"debug-dump",       required_argument, 0, 0},
      {"i2c-adapter",      required_argument, 0, 'd'},
      {"t68-file",         required_argument, 0, 0},
      {"t68-datatype",     required_argument, 0, 0},
      {"format",           no_argument,       0, 'f'},
      {"flash",            required_argument, 0, 0},
      {"firmware-version", required_argument, 0, 0},
      {"frames",           required_argument, 0, 0},
      {"help",             no_argument,       0, 'h'},
      {"instance",         required_argument, 0, 'I'},
      {"load",             required_argument, 0, 0},
      {"save",             required_argument, 0, 0},
      {"count",            required_argument, 0, 'n'},
      {"port",             required_argument, 0, 'p'},
      {"read",             no_argument,       0, 'R'},
      {"reset",            no_argument,       0, 0},
      {"reset-bootloader", no_argument,       0, 0},
      {"register",         required_argument, 0, 'r'},
      {"references",       no_argument,       0, 0},
      {"bridge-server",    no_argument,       0, 'S'},
      {"test",             no_argument,       0, 't'},
      {"type",             required_argument, 0, 'T'},
      {"verbose",          required_argument, 0, 'v'},
      {"version",          no_argument,       0, 0},
      {"write",            no_argument,       0, 'W'},
      {0,                  0,                 0,  0 }
    };

    c = getopt_long(argc, argv, "a:C:d:D:fghI:n:p:Rr:StT:v:W", long_options, &option_index);

    if (c == -1)
      break;

    switch (c)
    {
      case 0:
        if (!strcmp(long_options[option_index].name, "t68-file"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_SERIAL_DATA;
            strncpy(strbuf, optarg, sizeof(strbuf));
            strbuf[sizeof(strbuf) - 1] = '\0';
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "t68-datatype"))
        {
          t68_datatype = strtol(optarg, NULL, 0);
        }
        else if (!strcmp(long_options[option_index].name, "flash"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_FLASH;
            strncpy(strbuf, optarg, sizeof(strbuf));
            strbuf[sizeof(strbuf) - 1] = '\0';
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "backup"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_BACKUP;
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "calibrate"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_CALIBRATE;
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "debug-dump"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_DEBUG_DUMP;
            strncpy(strbuf, optarg, sizeof(strbuf));
            strbuf[sizeof(strbuf) - 1] = '\0';
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "reset"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_RESET;
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "load"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_LOAD_CFG;
            strncpy(strbuf, optarg, sizeof(strbuf));
            strbuf[sizeof(strbuf) - 1] = '\0';
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "save"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_SAVE_CFG;
            strncpy(strbuf, optarg, sizeof(strbuf));
            strbuf[sizeof(strbuf) - 1] = '\0';
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "reset-bootloader"))
        {
          if (cmd == CMD_NONE) {
            cmd = CMD_RESET_BOOTLOADER;
          } else {
            print_usage(argv[0]);
            return -1;
          }
        }
        else if (!strcmp(long_options[option_index].name, "firmware-version"))
        {
          strncpy(strbuf2, optarg, sizeof(strbuf2));
        }
        else if (!strcmp(long_options[option_index].name, "frames"))
        {
          t37_frames = strtol(optarg, NULL, 0);
        }
        else if (!strcmp(long_options[option_index].name, "references"))
        {
          t37_mode = REFS_MODE;
        }
        else if (!strcmp(long_options[option_index].name, "version"))
        {
          printf("mxt-app %s\n", __GIT_VERSION);
          return 0;
        }
        else
        {
          printf("Unknown option %s\n", long_options[option_index].name);
        }
        break;

      case 'a':
        if (optarg) {
          i2c_address = strtol(optarg, NULL, 16);
        }
        break;

      case 'd':
        if (optarg) {
          i2c_adapter = strtol(optarg, NULL, 0);
        }
        break;

      case 'C':
        if (cmd == CMD_NONE) {
          cmd = CMD_BRIDGE_CLIENT;
          strncpy(strbuf, optarg, sizeof(strbuf));
          strbuf[sizeof(strbuf) - 1] = '\0';
        } else {
          print_usage(argv[0]);
          return -1;
        }
        break;

      case 'g':
        if (cmd == CMD_NONE) {
          cmd = CMD_GOLDEN_REFERENCES;
        } else {
          print_usage(argv[0]);
          return -1;
        }
        break;

      case 'h':
        print_usage(argv[0]);
        return 0;

      case 'f':
        format = true;
        break;

      case 'I':
        if (optarg) {
          instance = strtol(optarg, NULL, 0);
        }
        break;

      case 'n':
        if (optarg) {
          count = strtol(optarg, NULL, 0);
        }
        break;

      case 'p':
        if (optarg) {
          port = strtol(optarg, NULL, 0);
        }
        break;

      case 'r':
        if (optarg) {
          address = strtol(optarg, NULL, 0);
        }
        break;

      case 'R':
        if (cmd == CMD_NONE) {
          cmd = CMD_READ;
        } else {
          print_usage(argv[0]);
          return -1;
        }
        break;

      case 'S':
        if (cmd == CMD_NONE) {
          cmd = CMD_BRIDGE_SERVER;
        } else {
          print_usage(argv[0]);
          return -1;
        }
        break;

      case 'T':
        if (optarg) {
          object_type = strtol(optarg, NULL, 0);
        }
        break;

      case 't':
        if (cmd == CMD_NONE) {
          cmd = CMD_TEST;
        } else {
          print_usage(argv[0]);
          return -1;
        }
        break;

      case 'v':
        if (optarg) {
          verbose = strtol(optarg, NULL, 0);
          mxt_set_verbose(verbose);
          LOG(LOG_DEBUG, "verbose:%u", verbose);
        }
        break;

      case 'W':
        if (cmd == CMD_NONE) {
          cmd = CMD_WRITE;
        } else {
          print_usage(argv[0]);
          return -1;
        }
        break;

      default:
        /* Output newline to create space under getopt error output */
        printf("\n\n");
        print_usage(argv[0]);
        return -1;
    }
  }

  /* Debug does not work until mxt_set_verbose() is called */
  LOG(LOG_DEBUG, "Version:%s", __GIT_VERSION);

  if (cmd == CMD_WRITE || cmd == CMD_READ)
  {
    LOG(LOG_DEBUG, "instance:%u", instance);
    LOG(LOG_DEBUG, "count:%u", count);
    LOG(LOG_DEBUG, "address:%u", address);
    LOG(LOG_DEBUG, "object_type:%u", object_type);
    LOG(LOG_DEBUG, "format:%s", format ? "true" : "false");
  }

  /* initialise chip, bootloader mode handles this itself */
  if (cmd != CMD_FLASH)
  {
    ret = mxt_init_chip(i2c_adapter, i2c_address);
    if (ret < 0)
      return ret;

    /*! Turn on kernel dmesg output of MSG */
    mxt_set_debug(true);
  }

  switch (cmd) {
    case CMD_WRITE:
      LOG(LOG_DEBUG, "Write command");

      if (object_type > 0) {
        object_address = get_object_address(object_type, instance);
        if (object_address == OBJECT_NOT_FOUND) {
          printf("No such object\n");
          ret = -1;
          break;
        }

        LOG(LOG_DEBUG, "T%u address:%u offset:%u", object_type,
            object_address, address);
        address = object_address + address;

        if (count == 0) {
          count = get_object_size(object_type);
        }
      } else if (count == 0) {
        printf("Not enough arguments!\n");
        return -1;
      }

      if (optind != (argc - 1)) {
        printf("Must give hex input\n");
        return -1;
      }

      ret = mxt_convert_hex(argv[optind], &databuf[0], &count, sizeof(databuf));
      if (ret < 0) {
        printf("Hex convert error\n");
      } else {
        ret = mxt_write_register(&databuf[0], address, count);
        if (ret < 0) {
          printf("Write error\n");
        }
      }
      break;

    case CMD_READ:
      LOG(LOG_DEBUG, "Read command");
      ret = read_object(object_type, instance, address, count, format);
      break;

    case CMD_GOLDEN_REFERENCES:
      LOG(LOG_DEBUG, "CMD_GOLDEN_REFERENCES");
      ret = mxt_store_golden_refs();
      break;

    case CMD_BRIDGE_SERVER:
      LOG(LOG_DEBUG, "CMD_BRIDGE_SERVER");
      LOG(LOG_DEBUG, "port:%u", port);
      ret = mxt_socket_server(port);
      break;

    case CMD_BRIDGE_CLIENT:
      LOG(LOG_DEBUG, "CMD_BRIDGE_CLIENT");
      ret = mxt_socket_client(strbuf, port);
      break;

    case CMD_SERIAL_DATA:
      LOG(LOG_DEBUG, "CMD_SERIAL_DATA");
      LOG(LOG_DEBUG, "t68_datatype:%u", t68_datatype);
      ret = mxt_serial_data_upload(strbuf, t68_datatype);
      break;

    case CMD_TEST:
      LOG(LOG_DEBUG, "CMD_TEST");
      ret = run_self_tests(SELF_TEST_ALL);
      break;

    case CMD_FLASH:
      LOG(LOG_DEBUG, "CMD_FLASH");
      ret = mxt_flash_firmware(strbuf, strbuf2, i2c_adapter, i2c_address);
      break;

    case CMD_RESET:
      LOG(LOG_DEBUG, "CMD_RESET");
      ret = mxt_reset_chip(false);
      break;

    case CMD_RESET_BOOTLOADER:
      LOG(LOG_DEBUG, "CMD_RESET_BOOTLOADER");
      ret = mxt_reset_chip(true);
      break;

    case CMD_BACKUP:
      LOG(LOG_DEBUG, "CMD_BACKUP");
      ret = mxt_backup_config();
      break;

    case CMD_CALIBRATE:
      LOG(LOG_DEBUG, "CMD_CALIBRATE");
      ret = mxt_calibrate_chip();
      break;

    case CMD_DEBUG_DUMP:
      LOG(LOG_DEBUG, "CMD_DEBUG_DUMP");
      LOG(LOG_DEBUG, "mode:%u", t37_mode);
      LOG(LOG_DEBUG, "frames:%u", t37_frames);
      ret = mxt_debug_dump(t37_mode, strbuf, t37_frames);
      break;

    case CMD_LOAD_CFG:
      LOG(LOG_DEBUG, "CMD_LOAD_CFG");
      LOG(LOG_DEBUG, "filename:%s", strbuf);
      ret = mxt_load_config_file(strbuf);
      if (ret < 0)
      {
        LOG(LOG_ERROR, "Error loading the configuration\n");
      }
      else
      {
        LOG(LOG_INFO, "Configuration loaded\n");

        ret = mxt_backup_config();
        if (ret < 0)
        {
          LOG(LOG_ERROR, "Error backing up\n");
        }
        else
        {
          LOG(LOG_INFO, "Configuration backed up\n");

          ret = mxt_reset_chip(false);
          if (ret < 0)
          {
            LOG(LOG_ERROR, "Error resetting\n");
          }
          else
          {
            LOG(LOG_INFO, "Chip reset\n");
          }
        }
      }
      break;

    case CMD_SAVE_CFG:
      LOG(LOG_DEBUG, "CMD_SAVE_CFG");
      LOG(LOG_DEBUG, "filename:%s", strbuf);
      ret = mxt_save_config_file(strbuf);
      break;

    case CMD_NONE:
    default:
      LOG(LOG_DEBUG, "cmd: %d", cmd);
      ret = mxt_menu();
      break;
  }

  mxt_set_debug(false);
  mxt_release();

  return ret;
}
