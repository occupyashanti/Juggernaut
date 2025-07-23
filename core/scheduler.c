// core/scheduler.c
#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define MAX_TASKS 1024
#define CHECKPOINT_INTERVAL 5 // seconds

struct scheduler {
    sched_task_t tasks[MAX_TASKS];
    int task_count;
    pthread_mutex_t lock;
    pthread_t thread;
    int running;
    void (*ai_hook)(void *);
    time_t last_checkpoint;
};

static void *scheduler_thread_func(void *arg);
static void scheduler_do_checkpoint(scheduler_t *sched, const char *path);

scheduler_t *scheduler_init(void) {
    scheduler_t *sched = (scheduler_t *)calloc(1, sizeof(scheduler_t));
    if (!sched) return NULL;
    pthread_mutex_init(&sched->lock, NULL);
    sched->task_count = 0;
    sched->running = 0;
    sched->ai_hook = NULL;
    sched->last_checkpoint = time(NULL);
    return sched;
}

void scheduler_start(scheduler_t *sched) {
    if (!sched) return;
    sched->running = 1;
    pthread_create(&sched->thread, NULL, scheduler_thread_func, sched);
}

void scheduler_stop(scheduler_t *sched) {
    if (!sched) return;
    sched->running = 0;
    pthread_join(sched->thread, NULL);
    pthread_mutex_destroy(&sched->lock);
    free(sched);
}

void scheduler_submit(scheduler_t *sched, const sched_task_t *task) {
    if (!sched || !task) return;
    pthread_mutex_lock(&sched->lock);
    if (sched->task_count < MAX_TASKS) {
        memcpy(&sched->tasks[sched->task_count], task, sizeof(sched_task_t));
        sched->task_count++;
    }
    pthread_mutex_unlock(&sched->lock);
}

void scheduler_checkpoint(scheduler_t *sched, const char *path) {
    scheduler_do_checkpoint(sched, path);
}

void scheduler_resume(scheduler_t *sched, const char *path) {
    // Minimal: just reset task count for now
    sched->task_count = 0;
    // TODO: Load from file
}

void scheduler_register_ai_hook(scheduler_t *sched, void (*ai_callback)(void *)) {
    if (sched) sched->ai_hook = ai_callback;
}

static void *scheduler_thread_func(void *arg) {
    scheduler_t *sched = (scheduler_t *)arg;
    while (sched->running) {
        pthread_mutex_lock(&sched->lock);
        for (int i = 0; i < sched->task_count; ++i) {
            // Dispatch task to device (stub)
            switch (sched->tasks[i].device) {
                case DEVICE_CPU:
                    // TODO: CPU execution
                    break;
                case DEVICE_GPU:
                    // TODO: GPU execution
                    break;
                case DEVICE_FPGA:
                    // TODO: FPGA execution
                    break;
                default:
                    break;
            }
        }
        sched->task_count = 0;
        pthread_mutex_unlock(&sched->lock);
        // AI hook for load balancing
        if (sched->ai_hook) sched->ai_hook(sched);
        // Checkpoint every 5 seconds
        time_t now = time(NULL);
        if (now - sched->last_checkpoint >= CHECKPOINT_INTERVAL) {
            scheduler_do_checkpoint(sched, "scheduler.chkpt");
            sched->last_checkpoint = now;
        }
        struct timespec ts = {0, 10000000}; // 10ms
        nanosleep(&ts, NULL);
    }
    return NULL;
}

static void scheduler_do_checkpoint(scheduler_t *sched, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    pthread_mutex_lock(&sched->lock);
    fwrite(&sched->task_count, sizeof(int), 1, f);
    fwrite(sched->tasks, sizeof(sched_task_t), sched->task_count, f);
    pthread_mutex_unlock(&sched->lock);
    fclose(f);
}
