#include "task.h"
#include "../memory/heap.h"

/* ─────────────────────────────────────────────────────────────────
 *  Global scheduler state
 *
 *  main_task     → The initial context (kernel_main before any
 *                  user task existed). Statically allocated because
 *                  it represents code that's already running.
 *
 *  current_task  → Points to whichever task is running right now.
 *                  Updated on every context switch.
 *
 *  task_list     → Head of the circular run queue. The last task
 *                  in the list points back to this one.
 *
 *  next_pid      → Source of unique task IDs. Starts at 1 because
 *                  main_task takes ID 0.
 * ───────────────────────────────────────────────────────────────── */

static task_t main_task;                 /* Task 0 — the kernel itself */
task_t* current_task = NULL;             /* Currently running task */
task_t* task_list = NULL;                /* Head of the circular list */
static uint32_t next_pid = 1;            /* Next available task ID */

/* ─────────────────────────────────────────────────────────────────
 *  task_init — register the current execution context as Task 0
 *
 *  Called once from kernel_main, before any create_task calls.
 *
 *  The "current execution context" is kernel_main's own stack and
 *  registers — whatever state the CPU has right now. We wrap it in
 *  a task_t so the scheduler can treat it like any other task.
 *
 *  Note: main_task.esp is left as 0 here. It will be populated
 *  automatically the first time switch_to_task() saves this task's
 *  ESP (when yielding to another task for the first time).
 * ───────────────────────────────────────────────────────────────── */
void task_init(void) {
    /* Register kernel_main's current context as Task 0 */
    main_task.id = 0;
    main_task.esp = 0;                  /* Saved on first switch */
    main_task.state = TASK_RUNNING;
    main_task.next = &main_task;        /* Circular list of one */

    current_task = &main_task;
    task_list = &main_task;
}

/* ─────────────────────────────────────────────────────────────────
 *  create_task — spawn a new task from an entry point function
 *
 *  Allocates:
 *    - A task_t structure (from the heap)
 *    - A 4 KB stack (from the heap)
 *
 *  Then crafts the initial stack so that the FIRST time
 *  switch_to_task() restores this task, it "returns" into
 *  `entry_point` as if the function had been called normally.
 *
 *  Returns the new task (READY state) or NULL on allocation failure.
 * ───────────────────────────────────────────────────────────────── */
task_t* create_task(void (*entry_point)(void)) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    if (!new_task) return NULL;

    uint32_t* stack = (uint32_t*)kmalloc(4096);
    if (!stack) { kfree(new_task); return NULL; }

    uint32_t* top = stack + 1024;

    /* Stack frame must match switch_to_task's pop order:
     *   popfd → pop ebp → pop edi → pop esi → pop edx → pop ecx → pop ebx → pop eax → ret
     *
     * Because we decrement `top` before storing, we write in
     * REVERSE pop order. First write ends up at highest address.
     */
    *(--top) = (uint32_t)entry_point;  /* EIP  — ret target */
    *(--top) = 0;                      /* EAX */
    *(--top) = 0;                      /* EBX */
    *(--top) = 0;                      /* ECX */
    *(--top) = 0;                      /* EDX */
    *(--top) = 0;                      /* ESI */
    *(--top) = 0;                      /* EDI */
    *(--top) = 0;                      /* EBP */
    *(--top) = 0x202;                  /* EFLAGS — IF=1 */

    new_task->id = next_pid++;
    new_task->esp = (uint32_t)top;
    new_task->stack = stack;
    new_task->state = TASK_READY;

    new_task->next = task_list->next;
    task_list->next = new_task;

    return new_task;
}

/* ─────────────────────────────────────────────────────────────────
 *  task_schedule — yield the CPU to the next task
 *
 *  Picks the next task in the circular list and switches to it.
 *  If the "next" task is the same as the current one (only one
 *  task exists), returns immediately without switching.
 *
 *  Cooperative: this is only called when a task explicitly yields,
 *  or (later) from the timer IRQ for preemption.
 *
 *  The switch itself is done by switch_to_task(), which:
 *    1. Saves the current task's callee-saved registers on its stack
 *    2. Saves the current ESP into old_task->esp (via the pointer)
 *    3. Loads the new ESP from current_task->esp
 *    4. Pops the new task's saved registers
 *    5. Returns to wherever the new task was suspended
 * ───────────────────────────────────────────────────────────────── */
void task_schedule(void) {
    /* Sanity check: nothing to do if the scheduler isn't initialized */
    if (!current_task) return;

    task_t* old_task = current_task;
    current_task = current_task->next;  /* Round-robin: next in circle */

    /* If the "next" is the same as the current (only one task),
     * skip the switch — avoids a pointless save/restore cycle. */
    if (old_task != current_task) {
        switch_to_task(&old_task->esp, current_task->esp);
    }
}