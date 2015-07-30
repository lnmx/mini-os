/* 
 ****************************************************************************
 * (C) 2005 - Grzegorz Milos - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: sched.c
 *      Author: Grzegorz Milos
 *     Changes: Robert Kaiser
 *              
 *        Date: Aug 2005
 * 
 * Environment: Xen Minimal OS
 * Description: simple scheduler for Mini-Os
 *
 * The scheduler is non-preemptive (cooperative), and schedules according 
 * to Round Robin algorithm.
 *
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include <mini-os/os.h>
#include <mini-os/hypervisor.h>
#include <mini-os/time.h>
#include <mini-os/mm.h>
#include <mini-os/types.h>
#include <mini-os/lib.h>
#include <mini-os/xmalloc.h>
#include <mini-os/list.h>
#include <mini-os/sched.h>
#include <mini-os/semaphore.h>


#ifdef SCHED_DEBUG
#define DEBUG(_f, _a...) \
    printk("MINI_OS(file=sched.c, line=%d) " _f "\n", __LINE__, ## _a)
#else
#define DEBUG(_f, _a...)    ((void)0)
#endif

MINIOS_TAILQ_HEAD(thread_list, struct thread);

struct thread *idle_thread = NULL;
static struct thread_list exited_threads = MINIOS_TAILQ_HEAD_INITIALIZER(exited_threads);
static struct thread_list thread_list = MINIOS_TAILQ_HEAD_INITIALIZER(thread_list);
static int threads_started;

struct thread *main_thread;

void inline print_runqueue(void)
{
    struct thread *th;
    MINIOS_TAILQ_FOREACH(th, &thread_list, thread_list)
    {
        printk("   Thread \"%s\", runnable=%d\n", th->name, is_runnable(th));
    }
    printk("\n");
}

void schedule(void)
{
    printk("no scheduler!\n");
    BUG();
}

struct thread* create_thread(char *name, void (*function)(void *), void *data)
{
    struct thread *thread;
    unsigned long flags;
    /* Call architecture specific setup. */
    thread = arch_create_thread(name, function, data);
    /* Not runable, not exited, not sleeping */
    thread->flags = 0;
    thread->wakeup_time = 0LL;
    set_runnable(thread);
    local_irq_save(flags);
    MINIOS_TAILQ_INSERT_TAIL(&thread_list, thread, thread_list);
    local_irq_restore(flags);
    return thread;
}

void exit_thread(void)
{
    unsigned long flags;
    struct thread *thread = current;
    printk("Thread \"%s\" exited.\n", thread->name);
    local_irq_save(flags);
    /* Remove from the thread list */
    MINIOS_TAILQ_REMOVE(&thread_list, thread, thread_list);
    clear_runnable(thread);
    /* Put onto exited list */
    MINIOS_TAILQ_INSERT_HEAD(&exited_threads, thread, thread_list);
    local_irq_restore(flags);
    /* Schedule will free the resources */
    while(1)
    {
        schedule();
        printk("schedule() returned!  Trying again\n");
    }
}

void block(struct thread *thread)
{
    thread->wakeup_time = 0LL;
    clear_runnable(thread);
}

void msleep(uint32_t millisecs)
{
    struct thread *thread = get_current();
    thread->wakeup_time = NOW()  + MILLISECS(millisecs);
    clear_runnable(thread);
    schedule();
}

void wake(struct thread *thread)
{
    thread->wakeup_time = 0LL;
    set_runnable(thread);
}

void idle_thread_fn(void *unused)
{
    threads_started = 1;
    while (1) {
        block(current);
        schedule();
    }
}

DECLARE_MUTEX(mutex);

void th_f1(void *data)
{
    struct timeval tv1, tv2;

    for(;;)
    {
        down(&mutex);
        printk("Thread \"%s\" got semaphore, runnable %d\n", current->name, is_runnable(current));
        schedule();
        printk("Thread \"%s\" releases the semaphore\n", current->name);
        up(&mutex);
        
        
        gettimeofday(&tv1, NULL);
        for(;;)
        {
            gettimeofday(&tv2, NULL);
            if(tv2.tv_sec - tv1.tv_sec > 2) break;
        }
                
        
        schedule(); 
    }
}

void th_f2(void *data)
{
    for(;;)
    {
        printk("Thread OTHER executing, data 0x%p\n", data);
        schedule();
    }
}



void init_sched(void)
{
    printk("Initialising scheduler\n");

    idle_thread = create_thread("Idle", idle_thread_fn, NULL);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
