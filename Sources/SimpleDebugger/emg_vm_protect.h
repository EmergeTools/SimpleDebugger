//
//  emg_vm_protect.h
//  SimpleDebugger
//
//  Created by Noah Martin on 12/10/24.
//

#import <mach/mach.h>

#ifndef EMG_VM_PROTECT
#define EMG_VM_PROTECT

#ifdef __cplusplus
extern "C" {
#endif

kern_return_t emg_vm_protect(mach_port_t target, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection);

#ifdef __cplusplus
}
#endif

#endif
