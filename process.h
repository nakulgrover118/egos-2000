// egos-2000/kernel/process.h
// Additions/changes for MLFQ + lifecycle stats

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#define MLFQ_LEVELS 5

struct process {
    int pid;
    int status;
    // ... existing registers and state fields ...

    /* ---------- Lifecycle statistics ---------- */
    uint64_t create_time_ms;       // time when process was created
    uint64_t first_scheduled_ms;   // time when first scheduled (-1 if not yet)
    uint64_t finish_time_ms;       // time when terminated
    uint64_t cpu_time_ms;          // accumulated CPU time
    uint64_t last_run_start_ms;    // last time the process began running (for accounting)
    uint64_t timer_interrupts;     // number of timer interrupts encountered for this process

    /* ---------- MLFQ fields ---------- */
    int mlfq_level;                // 0 (highest priority) .. 4 (lowest)
    uint64_t remaining_level_ms;   // remaining runtime on the current level before demotion
    bool is_runnable;              // whether process is runnable (existing field may differ)

    /* other fields... */
};

#endif // PROCESS_H
