// egos-2000/kernel/kernel.c
#include "process.h"
#include "kernel.h"
#include <stdint.h>
#include <stdio.h>

extern uint64_t get_time_ms(void); // replace with EGOS time tick API
extern struct process *proc_list[]; // array of process pointers (adjust name to actual)
extern int proc_count;              // number of processes in proc_list
extern int GPID_SHELL;              // pid of shell process (if provided)
extern void context_switch_to(struct process *p); // switch to process p (existing)
extern void kprintf(const char *fmt, ...);

static uint64_t last_mlfq_reset_ms = 0;
#define MLFQ_RESET_INTERVAL_MS 10000ULL // 10 seconds

/* intr_entry: called on interrupt. When timer interrupt occurs, update lifecycle stats
 * for currently running process (if any).
 */
void intr_entry(struct trapframe *tf) {
    // existing interrupt entry code...
    // Identify which interrupt; below we only handle timer accounting:
    if (is_timer_interrupt(tf)) {
        struct process *cur = current_process(); // adapt to your code's getter
        if (cur) {
            uint64_t now = get_time_ms();
            // If process was running, account cpu time since last_run_start_ms
            if (cur->last_run_start_ms != (uint64_t)-1) {
                uint64_t elapsed = (now >= cur->last_run_start_ms) ? (now - cur->last_run_start_ms) : 0;
                cur->cpu_time_ms += elapsed;
                // decrement the remaining time on this level
                if (cur->remaining_level_ms != (uint64_t)-1 && elapsed <= cur->remaining_level_ms)
                    cur->remaining_level_ms -= elapsed;
                else
                    cur->remaining_level_ms = 0;
                // reset last_run_start to now; scheduler will set again when resumed
                cur->last_run_start_ms = now;
            }
            cur->timer_interrupts++;
        }
    }

    // continue with existing interrupt handling (deliver device interrupts etc.)
}

/* proc_yield: choose next runnable process based on lowest mlfq_level.
 * Call mlfq_update_level() before choosing next process to apply demotions,
 * and mlfq_reset_level_all() every 10 seconds (or when shell input exists).
 */
void proc_yield(void) {
    // 1) Periodic reset of MLFQ levels
    uint64_t now = get_time_ms();
    if (now - last_mlfq_reset_ms >= MLFQ_RESET_INTERVAL_MS) {
        // reset all to level 0
        mlfq_reset_level_all(proc_list, proc_count);
        last_mlfq_reset_ms = now;
    }

    // Optionally, if keyboard input exists for shell, move shell to level 0:
    if (keyboard_has_input()) { // replace with actual keyboard check function
        struct process *shellp = proc_get_by_pid(GPID_SHELL); // adapt to your API
        if (shellp) {
            mlfq_reset_level(shellp);
            kprintf("[MLFQ] Shell forced to level 0 due to keyboard input\n");
        }
    }

    // Accurately update the level of all processes using their remaining_level_ms
    for (int i = 0; i < proc_count; i++) {
        struct process *p = proc_list[i];
        if (!p) continue;
        mlfq_update_level(p);
    }

    // 2) Choose runnable process with smallest level number (highest priority)
    struct process *best = NULL;
    for (int level = 0; level < MLFQ_LEVELS; level++) {
        // scan processes for runnable processes of this level
        for (int i = 0; i < proc_count; i++) {
            struct process *p = proc_list[i];
            if (!p) continue;
            if (!p->is_runnable) continue; // adapt to your 'runnable' check
            if (p->mlfq_level != level) continue;
            // choose this process (first match) - could apply more tie-breakers if needed
            best = p;
            break;
        }
        if (best) break;
    }

    if (best) {
        // Set response time if first scheduled
        if (best->first_scheduled_ms == (uint64_t)-1) {
            best->first_scheduled_ms = now;
        }
        // record run start time for bookkeeping
        best->last_run_start_ms = now;

        // switch context
        context_switch_to(best);

        // after the process yields or is preempted, we will return here
        // and timer accounting will be handled in intr_entry which updates cpu_time_ms
    } else {
        // no runnable process, idle or schedule idle thread
        schedule_idle();
    }
}
