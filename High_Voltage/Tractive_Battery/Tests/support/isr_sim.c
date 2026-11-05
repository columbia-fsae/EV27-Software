#include "isr_sim.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "hal_fakes.h"

// Held by the ISR thread while the handler runs, and by the main thread while
// interrupts are masked.
static pthread_mutex_t irq_lock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local bool masked;

static pthread_t isr_thread;
static atomic_bool running;
static atomic_uint_least32_t isr_runs;
static isr_sim_handler_t isr_handler;

void isr_sim_mask(void) {
    if (!masked) {
        pthread_mutex_lock(&irq_lock);
        masked = true;
    }
}

void isr_sim_unmask(void) {
    if (masked) {
        masked = false;
        pthread_mutex_unlock(&irq_lock);
    }
}

static uint32_t get_primask(void) { return masked ? 1U : 0U; }

static void set_primask(uint32_t primask) {
    if (primask & 1U) {
        isr_sim_mask();
    } else {
        isr_sim_unmask();
    }
}

void isr_sim_install_irq_fakes(void) {
    __disable_irq_fake.custom_fake = isr_sim_mask;
    __enable_irq_fake.custom_fake = isr_sim_unmask;
    __get_PRIMASK_fake.custom_fake = get_primask;
    __set_PRIMASK_fake.custom_fake = set_primask;
}

static void* isr_thread_main(void* arg) {
    (void)arg;
    while (atomic_load(&running)) {
        pthread_mutex_lock(&irq_lock);
        isr_handler();
        pthread_mutex_unlock(&irq_lock);
        atomic_fetch_add(&isr_runs, 1);
        sched_yield();  // mutexes aren't fair; give a masked main thread a turn
    }
    return NULL;
}

void isr_sim_start(isr_sim_handler_t handler) {
    isr_handler = handler;
    atomic_store(&isr_runs, 0);
    atomic_store(&running, true);
    pthread_create(&isr_thread, NULL, isr_thread_main, NULL);
}

uint32_t isr_sim_stop(void) {
    atomic_store(&running, false);
    pthread_join(isr_thread, NULL);
    return atomic_load(&isr_runs);
}
