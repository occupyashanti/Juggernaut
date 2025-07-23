// cli/main.c
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>

#define JUG_VERSION "1.0"
#define JUG_BUILD "2024-06-08"

static jug_global_opts_t g_opts = {0};
static int g_exit_code = 0;

static void print_help(void) {
    printf("Usage: juggernaut [global flags] <command> [command flags]\n");
    printf("Commands:\n");
    printf("  analyze <db|hashfile>      Inspect hash corpus\n");
    printf("  auto [--target=FILE]       Auto strategy: detect, AI, attack\n");
    printf("  crack <hashfile> [opts]    Explicit cracking job\n");
    printf("  bench [algo]               Benchmark devices\n");
    printf("  auth <subcmd>              Manage authorization\n");
    printf("  config <subcmd>            Show/dump/validate config\n");
    printf("  devices                    List detected devices\n");
    printf("  checkpoint save|load       Manual state ops\n");
    printf("  quit / exit                Leave REPL\n");
    printf("Global flags:\n");
    printf("  --config <file>            Override config path\n");
    printf("  --auth <file>              Proof-of-authorization token\n");
    printf("  --ack-license              Confirm restricted use\n");
    printf("  --devices cpu,gpu,fpga     Device mask\n");
    printf("  --resume <statefile>       Resume from checkpoint\n");
    printf("  --json                     JSON output\n");
    printf("  --no-color                 Disable ANSI colors\n");
    printf("  -v / -vv / -vvv            Verbosity\n");
    printf("  -h | --help                Show help\n");
    printf("  --version                  Print version info\n");
    printf("See docs/manual.md for full documentation.\n");
}

static void handle_sigint(int sig) {
    (void)sig;
    fprintf(stderr, "\nCaught SIGINT, shutting down...\n");
    // TODO: scheduler_cancel, checkpoint
    exit(130);
}

static void handle_sigusr1(int sig) {
    (void)sig;
    fprintf(stderr, "[SIGUSR1] Progress snapshot not implemented.\n");
}

static void merge_config(jug_global_opts_t *g) {
    // Minimal: just set defaults if not set
    if (!g->config_path) g->config_path = "config/juggernaut.yml";
    if (!g->auth_path) g->auth_path = "config/auth.token";
    if (!g->resume_path) g->resume_path = NULL;
    if (!g->device_mask) g->device_mask = 1; // CPU default
}

static int parse_device_mask(const char *s) {
    int mask = 0;
    if (strstr(s, "cpu")) mask |= 1;
    if (strstr(s, "gpu")) mask |= 2;
    if (strstr(s, "fpga")) mask |= 4;
    return mask;
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_sigint);
    signal(SIGUSR1, handle_sigusr1);
    memset(&g_opts, 0, sizeof(g_opts));
    int opt;
    int opt_idx = 0;
    static struct option long_opts[] = {
        {"config", required_argument, 0, 1},
        {"auth", required_argument, 0, 2},
        {"ack-license", no_argument, 0, 3},
        {"devices", required_argument, 0, 4},
        {"resume", required_argument, 0, 5},
        {"json", no_argument, 0, 6},
        {"no-color", no_argument, 0, 7},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 8},
        {0,0,0,0}
    };
    while ((opt = getopt_long(argc, argv, "hv", long_opts, &opt_idx)) != -1) {
        switch (opt) {
            case 1: g_opts.config_path = optarg; break;
            case 2: g_opts.auth_path = optarg; break;
            case 3: g_opts.ack_license = 1; break;
            case 4: g_opts.device_mask = parse_device_mask(optarg); break;
            case 5: g_opts.resume_path = optarg; break;
            case 6: g_opts.json_mode = 1; break;
            case 7: g_opts.no_color = 1; break;
            case 'v': g_opts.verbosity++; break;
            case 'h': print_help(); return 0;
            case 8: printf("Juggernaut v%s (%s)\n", JUG_VERSION, JUG_BUILD); return 0;
            default: print_help(); return 3;
        }
    }
    merge_config(&g_opts);
    // Ethics gating
    if (!g_opts.ack_license && !getenv("JUG_RUNTIME")) {
        FILE *f = fopen("config/ethics_policy.yml", "r");
        if (f) {
            char buf[4096];
            size_t n = fread(buf, 1, sizeof(buf)-1, f);
            buf[n] = 0;
            fclose(f);
            fprintf(stderr, "%s\n", buf);
        } else {
            fprintf(stderr, "Restricted use. See config/ethics_policy.yml\n");
        }
        fprintf(stderr, "You must acknowledge the license with --ack-license.\n");
        return 2;
    }
    // Remaining args: subcmd + subargs
    int sub_argc = argc - optind;
    char **sub_argv = argv + optind;
    if (sub_argc == 0 && isatty(0)) {
        return jug_repl(&g_opts);
    } else if (sub_argc == 0) {
        print_help();
        return 3;
    }
    // Dispatch subcommands
    g_opts.json_mode = g_opts.json_mode;
    g_opts.no_color = g_opts.no_color;
    if (strcmp(sub_argv[0], "analyze") == 0) g_exit_code = jug_cmd_analyze(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "auto") == 0) g_exit_code = jug_cmd_auto(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "crack") == 0) g_exit_code = jug_cmd_crack(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "bench") == 0) g_exit_code = jug_cmd_bench(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "auth") == 0) g_exit_code = jug_cmd_auth(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "config") == 0) g_exit_code = jug_cmd_config(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "devices") == 0) g_exit_code = jug_cmd_devices(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "checkpoint") == 0) g_exit_code = jug_cmd_checkpoint(sub_argc, sub_argv, &g_opts);
    else if (strcmp(sub_argv[0], "quit") == 0 || strcmp(sub_argv[0], "exit") == 0) return 0;
    else { print_help(); return 3; }
    // Map status to exit code
    switch (g_exit_code) {
        case JUG_CMD_OK: return 0;
        case JUG_CMD_NOAUTH: return 2;
        case JUG_CMD_BADARGS: return 3;
        case JUG_CMD_ERR: return 4;
        case JUG_CMD_INTERNAL: return 5;
        default: return 4;
    }
}
