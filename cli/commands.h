#ifndef JUG_COMMANDS_H
#define JUG_COMMANDS_H

#include <stddef.h>
#include <stdint.h>
#include "../core/hash_algorithms.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Global options for Juggernaut CLI.
 */
typedef struct {
    const char *config_path;
    const char *auth_path;
    int ack_license;
    uint32_t device_mask;      // bit0=CPU, bit1=GPU, bit2=FPGA
    int json_mode;
    int no_color;
    int verbosity;
    const char *resume_path;
} jug_global_opts_t;

/**
 * @brief Command status codes.
 */
typedef enum {
    JUG_CMD_OK = 0,
    JUG_CMD_ERR = 1,
    JUG_CMD_NOAUTH = 2,
    JUG_CMD_BADARGS = 3,
    JUG_CMD_INTERNAL = 4
} jug_cmd_status_t;

/**
 * @brief Target file info for hash cracking.
 */
typedef struct {
    char *path;
    hash_type_t detected_type;
    size_t count;
} jug_target_file_t;

/**
 * @brief Print Juggernaut banner.
 */
void jug_print_banner(void);

jug_cmd_status_t jug_cmd_analyze(int argc, char **argv, const jug_global_opts_t *g);
jug_cmd_status_t jug_cmd_auto(int argc, char **argv, const jug_global_opts_t *g);
jug_cmd_status_t jug_cmd_crack(int argc, char **argv, const jug_global_opts_t *g);
jug_cmd_status_t jug_cmd_bench(int argc, char **argv, const jug_global_opts_t *g);
jug_cmd_status_t jug_cmd_auth(int argc, char **argv, jug_global_opts_t *g_mut);
jug_cmd_status_t jug_cmd_config(int argc, char **argv, const jug_global_opts_t *g);
jug_cmd_status_t jug_cmd_devices(int argc, char **argv, const jug_global_opts_t *g);
jug_cmd_status_t jug_cmd_checkpoint(int argc, char **argv, const jug_global_opts_t *g);

/**
 * @brief Run the interactive REPL. Returns 0 on quit.
 */
int jug_repl(const jug_global_opts_t *startup_opts);

// Color macros
#ifndef COL_OK
#define COL_OK   "\x1b[32m"
#endif
#ifndef COL_ERR
#define COL_ERR  "\x1b[31m"
#endif
#ifndef COL_RST
#define COL_RST  "\x1b[0m"
#endif

// Core scheduler/AI stubs (extern, weak if missing)
typedef struct jug_job_spec jug_job_spec_t;
typedef struct jug_progress jug_progress_t;
__attribute__((weak)) int scheduler_submit(const jug_job_spec_t*, char *session_id_out, size_t);
__attribute__((weak)) int scheduler_poll(const char *session_id, jug_progress_t*);
__attribute__((weak)) int scheduler_cancel(const char *session_id);
__attribute__((weak)) int scheduler_checkpoint(const char *session_id, const char *path);
__attribute__((weak)) int scheduler_resume(const char *path, char *session_id_out, size_t);
__attribute__((weak)) int ai_suggest_strategy(const char *hashfile, char *out_buf, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif // JUG_COMMANDS_H 