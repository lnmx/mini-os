/******************************************************************************
 * hypervisor.c
 * 
 * Communication to/from hypervisor.
 * 
 * Copyright (c) 2002-2003, K A Fraser
 * Copyright (c) 2005, Grzegorz Milos, gm281@cam.ac.uk,Intel Research Cambridge
 * 
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
#include <mini-os/lib.h>
#include <mini-os/hypervisor.h>
#include <mini-os/events.h>

#define active_evtchns(cpu,sh,idx)              \
    ((sh)->evtchn_pending[idx] &                \
     ~(sh)->evtchn_mask[idx])

int in_callback;

/* Called from the interrupt handler when an event is pending.
 * This default implementation calls do_event immendiately (from within
 * the interrupt handler) for each pending event.
 *
 * If you want to handle events synchronously (e.g. checking for pending
 * events yourself just before blocking), override this with a version
 * that just clears evtchn_upcall_pending and returns.
 */
__attribute__((weak)) void do_hypervisor_callback(struct pt_regs *regs)
{
    // replaced by mirage-platform/bindings/eventchn_stubs.c
}

void force_evtchn_callback(void)
{
#ifdef XEN_HAVE_PV_UPCALL_MASK
    int save;
#endif
    vcpu_info_t *vcpu;
    vcpu = &HYPERVISOR_shared_info->vcpu_info[smp_processor_id()];
#ifdef XEN_HAVE_PV_UPCALL_MASK
    save = vcpu->evtchn_upcall_mask;
#endif

    while (vcpu->evtchn_upcall_pending) {
#ifdef XEN_HAVE_PV_UPCALL_MASK
        vcpu->evtchn_upcall_mask = 1;
#endif
        barrier();
        do_hypervisor_callback(NULL);
        barrier();
#ifdef XEN_HAVE_PV_UPCALL_MASK
        vcpu->evtchn_upcall_mask = save;
        barrier();
#endif
    };
}

inline void mask_evtchn(uint32_t port)
{
    shared_info_t *s = HYPERVISOR_shared_info;
    synch_set_bit(port, &s->evtchn_mask[0]);
}

inline void unmask_evtchn(uint32_t port)
{
    shared_info_t *s = HYPERVISOR_shared_info;
    vcpu_info_t *vcpu_info = &s->vcpu_info[smp_processor_id()];

    synch_clear_bit(port, &s->evtchn_mask[0]);

    /*
     * The following is basically the equivalent of 'hw_resend_irq'. Just like
     * a real IO-APIC we 'lose the interrupt edge' if the channel is masked.
     */
    if (  synch_test_bit        (port,    &s->evtchn_pending[0]) && 
         !synch_test_and_set_bit(port / (sizeof(unsigned long) * 8),
              &vcpu_info->evtchn_pending_sel) )
    {
        vcpu_info->evtchn_upcall_pending = 1;
#ifdef XEN_HAVE_PV_UPCALL_MASK
        if ( !vcpu_info->evtchn_upcall_mask )
#endif
            force_evtchn_callback();
    }
}

inline void clear_evtchn(uint32_t port)
{
    shared_info_t *s = HYPERVISOR_shared_info;
    synch_clear_bit(port, &s->evtchn_pending[0]);
}
