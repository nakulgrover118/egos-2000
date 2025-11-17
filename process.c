// egos-2000/kernel/process.c
#include "process.h"
#include "kernel.h" // for logging/printing/time functions etc.
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern uint64_t get_time_ms(void); // IMPORTANT: replace with EGOS time function if different
extern void kprintf(const char *fmt, ...); // kernel print routine (adjust name if needed)

/* Helper: compute level runtime cap in ms for level i: (i+1)*100 ms */
static inline uint64_t level_quanta_ms(int level) {
    return (uint64_t)(level + 1) * 100ULL;
}

/* Update the MLFQ level based on how much this process has run at its current level.
 * If process has consumed more than allowed for this level, demote it by +1 level
 * (unless already at max level).
 * This should be called after CPU time is accounted for (e.g. inside proc_yield()).
 */
void mlfq_update_level(struct process *p) {
    if (!p) return;
    // If process is already at max level, nothing to do.
    if (p->mlfq_level >= MLFQ_LEVELS - 1) {
        p->remaining_level_ms = (uint64_t)-1; // no further demotion
        return;
    }
    // If we don't have remaining_level_ms initialized, set it to the quota for this level
    if (p->remaining_level_ms == 0 || p->remaining_level_ms == (uint64_t)-1) {
        p->remaining_level_ms = level_quanta_ms(p->mlfq_level);
    }
    // If cpu_time_ms since last level promotion is >= quota -> demote
    // We'll use remaining_level_ms to track how much is left; if <= 0, demote.
    if ((int64_t)p->remaining_level_ms <= 0) {
        // demote
        p->mlfq_level++;
        if (p->mlfq_level >= MLFQ_LEVELS) p->mlfq_level = MLFQ_LEVELS - 1;
        // reset remaining runtime for new level
        p->remaining_level_ms = level_quanta_ms(p->mlfq_level);
        kprintf("[MLFQ] pid %d demoted to level %d\n", p->pid, p->mlfq_level);
    }
}

/* Reset all processes to level 0 (called ~every 10 seconds or on shell input).
 * This enforces the starvation-avoidance policy.
 */
void mlfq_reset_level_all(struct process *proc_list[], int proc_count) {
    for (int i = 0; i < proc_count; i++) {
        struct process *p = proc_list[i];
        if (!p) continue;
        p->mlfq_level = 0;
        p->remaining_level_ms = level_quanta_ms(0);
    }
}

/* If your code architecture calls for resetting a single process (e.g. GPID_SHELL),
 * provide this helper:
 */
void mlfq_reset_level(struct process *p) {
    if (!p) return;
    p->mlfq_level = 0;
    p->remaining_level_ms = level_quanta_ms(0);
}

/* proc_alloc: called when creating a process. Initialize lifecycle + mlfq fields.
 * Update the function in your code to set these fields.
 */
void proc_alloc(struct process *p) {
    // existing initialization...
    p->create_time_ms = get_time_ms();
    p->first_scheduled_ms = (uint64_t)-1;
    p->finish_time_ms = (uint64_t)-1;
    p->cpu_time_ms = 0;
    p->last_run_start_ms = (uint64_t)-1;
    p->timer_interrupts = 0;
    p->mlfq_level = 0; // all processes start at level 0
    p->remaining_level_ms = level_quanta_ms(0);
    p->is_runnable = true;
    // ... any other standard initialization
}

/* proc_free: called at process termination. Print lifecycle statistics.
 * Adapt to call this from the kernel when the process terminates.
 */
void proc_free(struct process *p) {
    if (!p) return;

    p->finish_time_ms = get_time_ms();

    uint64_t turnaround = (p->finish_time_ms >= p->create_time_ms) ? (p->finish_time_ms - p->create_time_ms) : 0;
    uint64_t response = (p->first_scheduled_ms != (uint64_t)-1 && p->first_scheduled_ms >= p->create_time_ms) ?
                        (p->first_scheduled_ms - p->create_time_ms) : (uint64_t)-1;

    kprintf("Process %d exiting: turnaround=%llu ms, response=%llu ms, cpu_time=%llu ms, timer_interrupts=%llu\n",
            p->pid,
            (unsigned long long)turnaround,
            (response == (uint64_t)-1) ? (unsigned long long)0 : (unsigned long long)response,
            (unsigned long long)p->cpu_time_ms,
            (unsigned long long)p->timer_interrupts
    );

    // existing cleanup code...
}
