// cli/commands.c
// TODO: Implement CLI command helpers for Juggernaut

#include "commands.h"
#include "auto_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

// --- Job Spec and Progress Stubs ---
typedef struct jug_job_spec {
    char hashfile[256];
    hash_type_t hash_type;
    char wordlist[256];
    char mask[64];
    char rules[256];
    int attack_mode; // 0=brute, 1=dict, 2=ai, 3=cloud
    int device_mask;
    int cost;
    int ai_complexity;
    char cloud_uri[256];
} jug_job_spec_t;

typedef struct jug_progress {
    size_t tested;
    double rate_hps;
    double percent;
    size_t cracked;
    int done;
} jug_progress_t;

// --- Helper: Poll and print progress ---
static void poll_and_print_progress(const char *session_id, int json_mode) {
    jug_progress_t prog = {0};
    int poll_count = 0;
    while (1) {
        scheduler_poll(session_id, &prog);
        if (json_mode) {
            printf("{\"progress\":{\"tested\":%zu,\"rate_hps\":%.2f,\"percent\":%.2f,\"cracked\":%zu}}\n", prog.tested, prog.rate_hps, prog.percent, prog.cracked);
        } else {
            printf("\rTested: %zu  Rate: %.2f H/s  Cracked: %zu  Progress: %.2f%%   ", prog.tested, prog.rate_hps, prog.cracked, prog.percent);
            fflush(stdout);
        }
        if (prog.done) break;
        poll_count++;
        struct timespec ts = {0, 500000000}; // 0.5s
        nanosleep(&ts, NULL);
        if (poll_count > 200) break; // 100s max for stub
    }
    if (!json_mode) printf("\n");
}

// --- Helper: Build job spec from args ---
static void build_job_spec(jug_job_spec_t *spec, int argc, char **argv, const jug_global_opts_t *g, int attack_mode) {
    memset(spec, 0, sizeof(*spec));
    strncpy(spec->hashfile, argc > 1 ? argv[1] : "", sizeof(spec->hashfile)-1);
    spec->attack_mode = attack_mode;
    spec->device_mask = g->device_mask;
    spec->ai_complexity = 7;
    // Defaults
    strcpy(spec->wordlist, "wordlists/top10k.txt");
    strcpy(spec->mask, "?a?a?a?a?a?a");
    strcpy(spec->rules, "");
    spec->cost = 10;
    // Try to auto-detect hash type
    jug_hash_guess_t guesses[2];
    size_t n = jug_auto_detect_file(spec->hashfile, guesses, 2);
    spec->hash_type = (n > 0) ? guesses[0].type : HASH_UNKNOWN;
    // Parse extra args for wordlist/mask/rules
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--wordlist") == 0 && i+1 < argc) strncpy(spec->wordlist, argv[++i], sizeof(spec->wordlist)-1);
        else if (strcmp(argv[i], "--mask") == 0 && i+1 < argc) strncpy(spec->mask, argv[++i], sizeof(spec->mask)-1);
        else if (strcmp(argv[i], "--rules") == 0 && i+1 < argc) strncpy(spec->rules, argv[++i], sizeof(spec->rules)-1);
        else if (strcmp(argv[i], "--cost") == 0 && i+1 < argc) spec->cost = atoi(argv[++i]);
        else if (strcmp(argv[i], "--ai-complexity") == 0 && i+1 < argc) spec->ai_complexity = atoi(argv[++i]);
        else if (strcmp(argv[i], "--cloud-uri") == 0 && i+1 < argc) strncpy(spec->cloud_uri, argv[++i], sizeof(spec->cloud_uri)-1);
    }
}

// --- Brute-force, Dictionary, AI Hybrid, Cloud Attack Logic ---
// --- Logging Helper ---
static void log_event(int level, const char *fmt, ...) {
    // Level: 0=INFO, 1=WARN, 2=ERR, 3=DEBUG
    // TODO: Parse logging.toml for config (stub: use g_opts.verbosity)
    extern jug_global_opts_t g_opts;
    if (level > g_opts.verbosity) return;
    FILE *f = fopen("logs/juggernaut.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

// --- Progress/Resume API ---
static int save_progress(const char *session_id, const char *path) {
    return scheduler_checkpoint(session_id, path);
}
static int load_progress(const char *path, char *session_id_out, size_t outlen) {
    return scheduler_resume(path, session_id_out, outlen);
}

// --- Update crack/auto to log and support resume ---
jug_cmd_status_t jug_cmd_crack(int argc, char **argv, const jug_global_opts_t *g) {
    if (argc < 2) {
        color_printf(COL_ERR, "Usage: crack <hashfile> [--wordlist WL] [--mask MASK] [--rules RULES] [--cost N] [--ai-complexity N] [--cloud-uri URI] [--resume FILE]\n");
        return JUG_CMD_BADARGS;
    }
    jug_job_spec_t spec;
    build_job_spec(&spec, argc, argv, g, 0);
    int mode = 0;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--mode") == 0 && i+1 < argc) {
            if (strcmp(argv[i+1], "brute") == 0) mode = 0;
            else if (strcmp(argv[i+1], "dict") == 0) mode = 1;
            else if (strcmp(argv[i+1], "ai") == 0) mode = 2;
            else if (strcmp(argv[i+1], "cloud") == 0) mode = 3;
            i++;
        }
    }
    spec.attack_mode = mode;
    if (mode == 2) {
        char ai_rules[256] = "";
        ai_suggest_strategy(spec.hashfile, ai_rules, sizeof(ai_rules));
        strncpy(spec.rules, ai_rules, sizeof(spec.rules)-1);
    }
    if (mode == 3 && spec.cloud_uri[0] == 0) {
        snprintf(spec.cloud_uri, sizeof(spec.cloud_uri), "s3://bucket/%s", spec.hashfile);
    }
    // Resume support
    const char *resume_file = NULL;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--resume") == 0 && i+1 < argc) resume_file = argv[++i];
    }
    char session_id[64] = "";
    int rc = 0;
    if (resume_file) {
        rc = load_progress(resume_file, session_id, sizeof(session_id));
        log_event(0, "[RESUME] Loaded session %s from %s", session_id, resume_file);
    } else {
        rc = scheduler_submit(&spec, session_id, sizeof(session_id));
        log_event(0, "[SUBMIT] Job %s: file=%s mode=%d", session_id, spec.hashfile, spec.attack_mode);
    }
    if (rc != 0) {
        color_printf(COL_ERR, "Failed to submit or resume job.\n");
        log_event(2, "[ERROR] Failed to submit/resume job for %s", spec.hashfile);
        return JUG_CMD_ERR;
    }
    color_printf(COL_OK, "[+] Job session: %s\n", session_id);
    poll_and_print_progress(session_id, g->json_mode);
    log_event(0, "[COMPLETE] Job %s complete", session_id);
    // Save checkpoint every 5s (stub: once at end)
    if (!resume_file) save_progress(session_id, "checkpoint.chkpt");
    color_printf(COL_OK, "[+] Attack complete.\n");
    return JUG_CMD_OK;
}

jug_cmd_status_t jug_cmd_auto(int argc, char **argv, const jug_global_opts_t *g) {
    if (argc < 2) {
        color_printf(COL_ERR, "Usage: auto --target=<file> [--ai-complexity N] [--mode brute|dict|ai|cloud] [--resume FILE]\n");
        return JUG_CMD_BADARGS;
    }
    const char *target = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--target=", 9) == 0) target = argv[i]+9;
        else if (strcmp(argv[i], "--target") == 0 && i+1 < argc) target = argv[++i];
    }
    if (!target) {
        color_printf(COL_ERR, "Missing --target for auto mode.\n");
        return JUG_CMD_BADARGS;
    }
    jug_job_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    strncpy(spec.hashfile, target, sizeof(spec.hashfile)-1);
    spec.attack_mode = 2;
    spec.device_mask = g->device_mask;
    spec.ai_complexity = 7;
    jug_hash_guess_t guesses[2];
    size_t n = jug_auto_detect_file(spec.hashfile, guesses, 2);
    spec.hash_type = (n > 0) ? guesses[0].type : HASH_UNKNOWN;
    char ai_rules[256] = "";
    ai_suggest_strategy(spec.hashfile, ai_rules, sizeof(ai_rules));
    strncpy(spec.rules, ai_rules, sizeof(spec.rules)-1);
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--wordlist") == 0 && i+1 < argc) strncpy(spec.wordlist, argv[++i], sizeof(spec.wordlist)-1);
        else if (strcmp(argv[i], "--mask") == 0 && i+1 < argc) strncpy(spec.mask, argv[++i], sizeof(spec.mask)-1);
        else if (strcmp(argv[i], "--rules") == 0 && i+1 < argc) strncpy(spec.rules, argv[++i], sizeof(spec.rules)-1);
        else if (strcmp(argv[i], "--cost") == 0 && i+1 < argc) spec.cost = atoi(argv[++i]);
        else if (strcmp(argv[i], "--ai-complexity") == 0 && i+1 < argc) spec.ai_complexity = atoi(argv[++i]);
        else if (strcmp(argv[i], "--cloud-uri") == 0 && i+1 < argc) strncpy(spec.cloud_uri, argv[++i], sizeof(spec.cloud_uri)-1);
        else if (strcmp(argv[i], "--mode") == 0 && i+1 < argc) {
            if (strcmp(argv[i+1], "brute") == 0) spec.attack_mode = 0;
            else if (strcmp(argv[i+1], "dict") == 0) spec.attack_mode = 1;
            else if (strcmp(argv[i+1], "ai") == 0) spec.attack_mode = 2;
            else if (strcmp(argv[i+1], "cloud") == 0) spec.attack_mode = 3;
            i++;
        }
    }
    // Resume support
    const char *resume_file = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--resume") == 0 && i+1 < argc) resume_file = argv[++i];
    }
    char session_id[64] = "";
    int rc = 0;
    if (resume_file) {
        rc = load_progress(resume_file, session_id, sizeof(session_id));
        log_event(0, "[RESUME] Loaded session %s from %s", session_id, resume_file);
    } else {
        rc = scheduler_submit(&spec, session_id, sizeof(session_id));
        log_event(0, "[SUBMIT] Job %s: file=%s mode=%d", session_id, spec.hashfile, spec.attack_mode);
    }
    if (rc != 0) {
        color_printf(COL_ERR, "Failed to submit or resume job.\n");
        log_event(2, "[ERROR] Failed to submit/resume job for %s", spec.hashfile);
        return JUG_CMD_ERR;
    }
    color_printf(COL_OK, "[+] Job session: %s\n", session_id);
    poll_and_print_progress(session_id, g->json_mode);
    log_event(0, "[COMPLETE] Job %s complete", session_id);
    if (!resume_file) save_progress(session_id, "checkpoint.chkpt");
    color_printf(COL_OK, "[+] Auto attack complete.\n");
    return JUG_CMD_OK;
}

jug_cmd_status_t jug_cmd_bench(int argc, char **argv, const jug_global_opts_t *g) {
    color_printf(COL_OK, "[stub] Bench mode not yet implemented.\n");
    return JUG_CMD_OK;
}

jug_cmd_status_t jug_cmd_auth(int argc, char **argv, jug_global_opts_t *g_mut) {
    color_printf(COL_OK, "[stub] Auth command not yet implemented.\n");
    return JUG_CMD_OK;
}

jug_cmd_status_t jug_cmd_config(int argc, char **argv, const jug_global_opts_t *g) {
    color_printf(COL_OK, "[stub] Config command not yet implemented.\n");
    return JUG_CMD_OK;
}

// --- Device Detection Helper ---
static void detect_devices(int *cpu_threads, int *gpu_count) {
    // Detect CPU threads
    FILE *f = fopen("/proc/cpuinfo", "r");
    int cpus = 0;
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "processor", 9) == 0) cpus++;
        }
        fclose(f);
    }
    if (cpus == 0) cpus = 4; // fallback
    *cpu_threads = cpus;
    // Detect NVIDIA GPUs using nvidia-smi
    int gpus = 0;
    int status = system("nvidia-smi -L > /dev/null 2>&1");
    if (status == 0) {
        FILE *fp = popen("nvidia-smi -L | wc -l", "r");
        if (fp) {
            char buf[16];
            if (fgets(buf, sizeof(buf), fp)) gpus = atoi(buf);
            pclose(fp);
        }
    }
    *gpu_count = gpus;
}

/**
 * @brief List detected CPU/GPU/FPGA devices and capabilities.
 */
jug_cmd_status_t jug_cmd_devices(int argc, char **argv, const jug_global_opts_t *g) {
    int cpu_threads = 0, gpu_count = 0;
    detect_devices(&cpu_threads, &gpu_count);
    if (g->json_mode) {
        printf("{\"ok\":true,\"devices\":[{\"type\":\"CPU\",\"threads\":%d},{\"type\":\"GPU\",\"count\":%d}]}\n", cpu_threads, gpu_count);
    } else {
        color_printf(COL_OK, "Detected devices:\n");
        printf("  CPU threads: %d\n", cpu_threads);
        printf("  NVIDIA GPUs: %d\n", gpu_count);
        printf("  FPGA: (not implemented)\n");
    }
    return JUG_CMD_OK;
}

// --- Checkpoint Command ---
jug_cmd_status_t jug_cmd_checkpoint(int argc, char **argv, const jug_global_opts_t *g) {
    if (argc < 2) {
        color_printf(COL_ERR, "Usage: checkpoint save|load <file>\n");
        return JUG_CMD_BADARGS;
    }
    if (strcmp(argv[1], "save") == 0 && argc > 2) {
        // Save current session (stub: use last session_id)
        int rc = save_progress("last_session", argv[2]);
        if (rc == 0) color_printf(COL_OK, "Checkpoint saved to %s\n", argv[2]);
        else color_printf(COL_ERR, "Failed to save checkpoint.\n");
        return rc == 0 ? JUG_CMD_OK : JUG_CMD_ERR;
    } else if (strcmp(argv[1], "load") == 0 && argc > 2) {
        char session_id[64] = "";
        int rc = load_progress(argv[2], session_id, sizeof(session_id));
        if (rc == 0) color_printf(COL_OK, "Checkpoint loaded. Session: %s\n", session_id);
        else color_printf(COL_ERR, "Failed to load checkpoint.\n");
        return rc == 0 ? JUG_CMD_OK : JUG_CMD_ERR;
    } else {
        color_printf(COL_ERR, "Usage: checkpoint save|load <file>\n");
        return JUG_CMD_BADARGS;
    }
}

static int parse_line(char *line, char **argv) {
    int argc = 0;
    char *p = line;
    while (*p && argc < JUG_MAX_ARGS) {
        while (isspace(*p)) ++p;
        if (!*p) break;
        if (*p == '#') break;
        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"') ++p;
            if (*p) *p++ = 0;
        } else {
            argv[argc++] = p;
            while (*p && !isspace(*p) && *p != '#') ++p;
            if (*p) *p++ = 0;
        }
    }
    return argc;
}

int jug_repl(const jug_global_opts_t *startup_opts) {
    char line[JUG_MAX_LINE];
    char *argv[JUG_MAX_ARGS];
    int argc;
    jug_global_opts_t g = *startup_opts;
    g_json_mode = g.json_mode;
    g_no_color = g.no_color;
    jug_print_banner();
    while (1) {
        printf("Juggernaut> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        argc = parse_line(line, argv);
        if (argc == 0) continue;
        if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) break;
        if (strcmp(argv[0], "analyze") == 0) jug_cmd_analyze(argc, argv, &g);
        else if (strcmp(argv[0], "auto") == 0) jug_cmd_auto(argc, argv, &g);
        else if (strcmp(argv[0], "crack") == 0) jug_cmd_crack(argc, argv, &g);
        else if (strcmp(argv[0], "bench") == 0) jug_cmd_bench(argc, argv, &g);
        else if (strcmp(argv[0], "auth") == 0) jug_cmd_auth(argc, argv, &g);
        else if (strcmp(argv[0], "config") == 0) jug_cmd_config(argc, argv, &g);
        else if (strcmp(argv[0], "devices") == 0) jug_cmd_devices(argc, argv, &g);
        else if (strcmp(argv[0], "checkpoint") == 0) jug_cmd_checkpoint(argc, argv, &g);
        else color_printf(COL_ERR, "Unknown command: %s\n", argv[0]);
    }
    return 0;
}
