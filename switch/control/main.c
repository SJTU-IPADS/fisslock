
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <bf_rt/bf_rt.h>
#include <bfutils/clish/thread.h>
#include <bf_switchd/bf_switchd.h>
#include <bfsys/bf_sal/bf_sys_intf.h>

#include "bfrt.h"

bf_switchd_context_t *switchd_ctx;
char setup_script[64];

enum opts {
  OPT_INSTALLDIR = 1,
  OPT_BFSCRIPT,
  OPT_CONFFILE,
};

static struct option options[] = {
  {"help", no_argument, 0, 'h'},
  {"install-dir", required_argument, 0, OPT_INSTALLDIR},
  {"setup-script", required_argument, 0, OPT_BFSCRIPT},
  {"conf-file", required_argument, 0, OPT_CONFFILE},
};

static void parse_options(int argc, char **argv) {
  int option_index = 0;
  while (1) {
    int c = getopt_long(argc, argv, "h", options, &option_index);

    if (c == -1) {
      break;
    }
    switch (c) {
      case OPT_INSTALLDIR:
        switchd_ctx->install_dir = strdup(optarg);
        printf("Install Dir: %s\n", switchd_ctx->install_dir);
        break;
      case OPT_BFSCRIPT:
        sprintf(setup_script, "%s/bin/bfshell -f %s", 
          getenv("SDE_INSTALL"), optarg);
        printf("Port setup script : %s\n", optarg);
        break;
      case OPT_CONFFILE:
        switchd_ctx->conf_file = strdup(optarg);
        printf("Conf-file : %s\n", switchd_ctx->conf_file);
        break;
      case 'h':
      case '?':
        printf("fisslock_decider \n");
        printf(
            "Usage : fisslock_decider --install-dir <path to where the SDE is "
            "installed> --conf-file <full path to the conf file "
            "\n");
        exit(c == 'h' ? 0 : 1);
        break;
      default:
        printf("Invalid option\n");
        exit(0);
        break;
    }
  }

  if (switchd_ctx->install_dir == NULL) {
    printf("ERROR : --install-dir must be specified\n");
    exit(0);
  }

  if (switchd_ctx->conf_file == NULL) {
    printf("ERROR : --conf-file must be specified\n");
    exit(0);
  }

}

int main(int argc, char **argv) {

  if (!(switchd_ctx = calloc(1, sizeof(bf_switchd_context_t)))) {
    printf("Cannot Allocate switchd context\n");
    exit(1);
  }

  // Initialize the barefoot switchd library.
  parse_options(argc, argv);
  switchd_ctx->running_in_background = true;
  bf_switchd_lib_init(switchd_ctx);

  // Initialize MAT tables and their entries.
  printf("Initialize bfrt driver\n");
  driver_init();

  // Run the port setup script.
  printf("Execute the port setup script\n");
  system((const char*)setup_script);

  // Enter the interactive bfshell.
  // cli_run_bfshell();

  free(switchd_ctx);
  return 0;
}