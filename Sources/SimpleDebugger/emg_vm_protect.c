//
//  emg_vm_protect.c
//  SimpleDebugger
//
//  Created by Noah Martin on 12/10/24.
//

#import "emg_vm_protect.h"
#import <mach/mach.h>

#if defined(__arm64__) || defined(__aarch64__)

extern kern_return_t _kern_rpc_emg_vm_prot_trap(
  mach_port_name_t target,
  mach_vm_address_t address,
  mach_vm_size_t size,
  boolean_t set_maximum,
  vm_prot_t new_protection
  );

asm(
    ".globl __kern_rpc_emg_vm_prot_trap\n"
    ".text\n"
    ".align 2\n"
    "__kern_rpc_emg_vm_prot_trap:\n"
    "    mov x16, #-14\n"
    "    svc 0x80\n"
    "    ret\n"
);

kern_return_t emg_vm_protect(mach_port_t target, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection) {
  return _kern_rpc_emg_vm_prot_trap(target, address, size, set_maximum, new_protection);
}

#endif
