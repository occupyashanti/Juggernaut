// core/scheduler.h
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported compute device types.
 */
typedef enum {
    DEVICE_CPU,
    DEVICE_GPU,
    DEVICE_FPGA,
    DEVICE_UNKNOWN
} device_type_t;

/**
 * @brief Scheduler task structure.
 */
typedef struct {
    device_type_t device;
    void *task_data;
    size_t task_size;
    int priority;
    int task_id;
} sched_task_t;

/**
 * @brief Scheduler context structure.
 */
typedef struct scheduler scheduler_t;

/**
 * @brief Initialize the scheduler.
 * @return Pointer to scheduler context
 */
scheduler_t *scheduler_init(void);

/**
 * @brief Start the scheduler (spawns threads, begins processing).
 * @param sched Scheduler context
 */
void scheduler_start(scheduler_t *sched);

/**
 * @brief Stop the scheduler and free resources.
 * @param sched Scheduler context
 */
void scheduler_stop(scheduler_t *sched);

/**
 * @brief Submit a task to the scheduler.
 * @param sched Scheduler context
 * @param task Task to submit
 */
void scheduler_submit(scheduler_t *sched, const sched_task_t *task);

/**
 * @brief Checkpoint the scheduler state (for resume).
 * @param sched Scheduler context
 * @param path Path to checkpoint file
 */
void scheduler_checkpoint(scheduler_t *sched, const char *path);

/**
 * @brief Resume scheduler from checkpoint.
 * @param sched Scheduler context
 * @param path Path to checkpoint file
 */
void scheduler_resume(scheduler_t *sched, const char *path);

/**
 * @brief Register an AI hook for dynamic load balancing.
 * @param sched Scheduler context
 * @param ai_callback Callback function
 */
void scheduler_register_ai_hook(scheduler_t *sched, void (*ai_callback)(void *));

#ifdef __cplusplus
}
#endif

#endif // SCHEDULER_H 