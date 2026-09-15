#ifndef TASK_H
#define TASK_H

/* Include guard: prevents double-inclusion in the same translation unit. */

#include "../lib/stdint.h"   /* For uint32_t */

/* ─────────────────────────────────────────────────────────────────
 *  Cooperative Task Scheduler — data structures and API
 *
 *  This is a MINIMAL task abstraction: no preemption, no priorities,
 *  no separate address spaces. Tasks:
 *    - Share the same memory (kernel space)
 *    - Run one at a time on a single CPU
 *    - Voluntarily yield by calling task_schedule()
 *
 *  This is the first step toward multitasking. Later, you can add:
 *    - Preemption (yield from the timer IRQ)
 *    - Priorities and time slices
 *    - Separate page directories per task (processes)
 *    - User mode (ring 3)
 * ───────────────────────────────────────────────────────────────── */

/* ── Task state ───────────────────────────────────────────────── */

/* The lifecycle states a task can be in.
 *
 *   READY       → Runnable, waiting for CPU time
 *   RUNNING     → Currently executing on the CPU
 *   TERMINATED  → Finished, no longer scheduled (zombie)
 *
 * Real kernels have more states (BLOCKED, SLEEPING, WAITING).
 * Kept minimal here — the scheduler is cooperative, so tasks
 * don't block on I/O or wait for events yet. */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_TERMINATED
} task_state_t;

/* ─────────────────────────────────────────────────────────────────
 *  Task Control Block (TCB)
 *
 *  Represents one task in the scheduler. Contains everything the
 *  kernel needs to pause and resume the task:
 *
 *      ┌─────────────────────────────────────┐
 *      │ id                                     │  ← unique task ID
 *      │ esp                                    │  ← saved stack pointer
 *      │ stack ──────────► [ task's stack ]    │  ← heap-allocated
 *      │ state                                  │  ← READY/RUNNING/...
 *      │ next ──────────► (next task in list)  │  ← circular list
 *      └─────────────────────────────────────┘
 *
 *  The `esp` field is the key: when the scheduler switches away
 *  from a task, it saves the task's current stack pointer here.
 *  When it switches back, it restores esp — and the task resumes
 *  exactly where it left off.
 * ───────────────────────────────────────────────────────────────── */

typedef struct task {
    uint32_t id;                /* Unique identifier (assigned by create_task) */
    uint32_t esp;               /* Saved stack pointer (updated on context switch) */
    uint32_t* stack;            /* Base of the task's heap-allocated stack */
    task_state_t state;         /* READY / RUNNING / TERMINATED */
    struct task* next;          /* Next task in the circular run queue */
} task_t;

/* ─────────────────────────────────────────────────────────────────
 *  Context switch primitive (implemented in assembly)
 *
 *  This is the low-level function that actually saves one task's
 *  CPU context and restores another's.
 *
 *  Prototype:  switch_to_task(uint32_t* old_esp, uint32_t new_esp)
 *
 *  In assembly (conceptually):
 *      1. Push callee-saved registers (ebx, esi, edi, ebp) onto
 *         the current stack.
 *      2. Save the current ESP into *old_esp.
 *      3. Load ESP from new_esp.
 *      4. Pop the new task's saved registers.
 *      5. `ret` — returns to wherever the new task was suspended.
 *
 *  Note the asymmetry: `old_esp` is a POINTER (we write to it),
 *  while `new_esp` is a VALUE (we load it directly). This is the
 *  signature xv6 and many tutorials use.
 *
 *  Callers don't invoke this directly — task_schedule() does. */
extern void switch_to_task(uint32_t* old_esp, uint32_t new_esp);

/* ─────────────────────────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────────────────────────── */

/* Initialize the task subsystem.
 *
 * Currently:
 *   - Creates the "idle"/main task representing the kernel's
 *     initial execution context (before any user task existed).
 *   - Sets up the run queue with the main task as the only entry.
 *
 * Called once from kernel_main, after heap_init (needs kmalloc
 * to allocate task structures and stacks). */
void task_init(void);

/* Create a new task that starts executing `entry_point`.
 *
 * Allocates:
 *   - A `task_t` structure (from the kernel heap)
 *   - A stack (typically 4 KB, from the kernel heap)
 *   - An initial stack frame that, when switched to, "returns"
 *     into `entry_point` as if it had been called normally.
 *
 * The new task starts in the READY state and is appended to the
 * run queue.
 *
 * Returns a pointer to the new task, or NULL on failure (out of
 * memory). If the task terminates, its resources must be freed
 * manually — there's no automatic reaping yet. */
task_t* create_task(void (*entry_point)(void));

/* Yield the CPU to the next runnable task.
 *
 * Called voluntarily by tasks that want to share CPU time, or
 * from the timer IRQ once preemption is added. Behavior:
 *   1. Pick the next READY task from the run queue (round-robin).
 *   2. Mark the current task READY (or TERMINATED if it exited).
 *   3. Call switch_to_task(&current->esp, next->esp).
 *
 * When this call returns, the calling task has been resumed —
 * possibly much later, possibly after many other tasks ran.
 *
 * Cooperative scheduling: if a task never calls this, it runs
 * forever and starves everyone else. Later, the timer IRQ will
 * call this automatically to preempt running tasks. */
void task_schedule(void);

#endif  /* TASK_H */