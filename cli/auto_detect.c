// cli/auto_detect.c
// TODO: Implement hash type auto-detection logic

#include "auto_detect.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <ctype.h>

#define MAX_SAMPLE_LINES 100
#define MAX_HASH_TYPES 8

// Regex patterns for known hash types
static const struct {
    hash_type_t type;
    const char *name;
    const char *pattern;
} hash_patterns[] = {
    { HASH_MD5,     "MD5",     "^[a-fA-F0-9]{32}$" },
    { HASH_BCRYPT,  "BCRYPT",  "^\$2[aby]\$[0-9]{2}\$[./A-Za-z0-9]{53}$" },
    { HASH_SHA256,  "SHA256",  "^[a-fA-F0-9]{64}$" },
    { HASH_SHA256,  "SHA256CRYPT", "^\$5\$.*" },
    { HASH_UNKNOWN, "HEX64",   "^[a-fA-F0-9]{64}$" },
    { HASH_UNKNOWN, "HEX40",   "^[a-fA-F0-9]{40}$" },
    { HASH_UNKNOWN, "HEX128",  "^[a-fA-F0-9]{128}$" },
};

static int match_regex(const char *str, const char *pattern) {
    regex_t re;
    int rc = regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) return 0;
    rc = regexec(&re, str, 0, NULL, 0);
    regfree(&re);
    return rc == 0;
}

static void tally_line(const char *line, int *scores, size_t npat) {
    for (size_t i = 0; i < npat; ++i) {
        if (match_regex(line, hash_patterns[i].pattern))
            scores[i]++;
    }
}

size_t jug_auto_detect_buffer(const char *buf, size_t len, jug_hash_guess_t *out, size_t max_out) {
    int scores[MAX_HASH_TYPES] = {0};
    size_t npat = sizeof(hash_patterns)/sizeof(hash_patterns[0]);
    size_t lines = 0;
    const char *p = buf, *end = buf + len;
    char line[256];
    while (p < end && lines < MAX_SAMPLE_LINES) {
        size_t l = 0;
        while (p < end && *p != '\n' && l < sizeof(line)-1) line[l++] = *p++;
        line[l] = 0;
        if (*p == '\n') ++p;
        if (l > 0 && !isspace(line[0])) {
            tally_line(line, scores, npat);
            ++lines;
        }
    }
    // Score normalization
    double total = 0.0;
    for (size_t i = 0; i < npat; ++i) total += scores[i];
    size_t nout = 0;
    for (size_t i = 0; i < npat && nout < max_out; ++i) {
        if (scores[i] > 0) {
            out[nout].type = hash_patterns[i].type;
            out[nout].confidence = total > 0 ? (double)scores[i]/total : 0.0;
            nout++;
        }
    }
    return nout;
}

size_t jug_auto_detect_file(const char *path, jug_hash_guess_t *out, size_t max_out) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[256*MAX_SAMPLE_LINES];
    size_t n = fread(buf, 1, sizeof(buf)-1, f);
    fclose(f);
    buf[n] = 0;
    return jug_auto_detect_buffer(buf, n, out, max_out);
}
