//
//  SimpleDebugger.h
//  SimpleDebugger
//
//  Created by Noah Martin on 10/9/24.
//

#if defined(__arm64__) || defined(__aarch64__)

#include "SimpleDebugger.h"

#import <pthread.h>
#import <mutex>
#import <mach/mach.h>
#import <libgen.h>
#import <os/log.h>
#import <mach-o/dyld_images.h>

#include "mach_messages.h"

#include <mach/exception.h>
#include <mach/arm/thread_state.h>

SimpleDebugger::SimpleDebugger() : exceptionPort(MACH_PORT_NULL) {}

void replace_image_notifier(enum dyld_image_mode mode, uint32_t infoCount, const struct dyld_image_info info[]) { }

bool SimpleDebugger::startDebugging() {
  struct task_dyld_info dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  task_info(mach_task_self_, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count);
  struct dyld_all_image_infos *infos = (struct dyld_all_image_infos *)dyld_info.all_image_info_addr;
  infos->notification = replace_image_notifier;

  if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exceptionPort) != KERN_SUCCESS) {
    return false;
  }

  if (mach_port_insert_right(mach_task_self(), exceptionPort, exceptionPort, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
    return false;
  }

  if (task_set_exception_ports(
    mach_task_self(),
    // Register for EXC_MASK_BAD_ACCESS to catch cases where a thread
    // is trying to access a page that we are in the middle of changing.
    // It temporarily has execute permissions removed so could trigger this.
    // When it is triggered we should ignore it and retry the original instruction.
    EXC_MASK_BREAKPOINT | EXC_MASK_BAD_ACCESS,
    exceptionPort,
    EXCEPTION_DEFAULT,
    ARM_THREAD_STATE64) != KERN_SUCCESS) {
      return false;
  }

  m.lock();
  pthread_create(&serverThread, nullptr, &SimpleDebugger::exceptionServerWrapper, this);
  // Prevent returning until the server thread has started
  m.lock();
  return true;
}

void SimpleDebugger::setExceptionCallback(ExceptionCallback callback) {
    exceptionCallback = std::move(callback);
}

#define ARM64_BREAK_INSTRUCTION 0xD4200000

void protectPage(vm_address_t address, vm_size_t size, vm_prot_t newProtection) {
  kern_return_t result = vm_protect(mach_task_self(), address, size, 0, newProtection);

  if (result != 0) {
    printf("error calling vm_protect: %s (response value: %d)\n", mach_error_string(result), result);
  }
}

uint32_t setInstruction(vm_address_t address, uint32_t newInst) {
  uint32_t instruction = *((uint32_t *)address);
  thread_act_array_t threads;
  mach_msg_type_number_t thread_count;
  if (task_threads(mach_task_self(), &threads, &thread_count) != KERN_SUCCESS) {
    thread_count = 0;
  }

  thread_t myThread = mach_thread_self();
  for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
    if (threads[i] != myThread) {
      thread_suspend(threads[i]);
    }
  }
  protectPage(address, 1, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
  *(uint32_t *)address = newInst;
  protectPage(address, 1, VM_PROT_READ | VM_PROT_EXECUTE);

  for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
    if (threads[i] != myThread) {
      thread_resume(threads[i]);
    }
  }

  vm_size_t size = thread_count * sizeof(thread_t);
  vm_deallocate(mach_task_self(), (vm_address_t) threads, size);
  return instruction;
}

void SimpleDebugger::setBreakpoint(vm_address_t address) {
  uint32_t instruction = setInstruction(address, ARM64_BREAK_INSTRUCTION);
  originalInstruction.insert({address, instruction});
}

SimpleDebugger::~SimpleDebugger() {
  // TODO: Handle stopping the exception server
}

void* SimpleDebugger::exceptionServerWrapper(void* arg) {
  return static_cast<SimpleDebugger*>(arg)->exceptionServer();
}

void* SimpleDebugger::exceptionServer() {
  MachExceptionMessage exceptionMessage = {{0}};

  os_log(OS_LOG_DEFAULT, "Exception server started");

  m.unlock();
  while (true) {
    kern_return_t kr = mach_msg(&exceptionMessage.header,
                                MACH_RCV_MSG,
                                0,
                                sizeof(exceptionMessage),
                                exceptionPort,
                                MACH_MSG_TIMEOUT_NONE,
                                MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
      os_log(OS_LOG_DEFAULT, "Error receiving message");
      continue;
    }

    if (exceptionMessage.exception == EXC_BREAKPOINT) {
      mach_port_t thread = exceptionMessage.thread.name;
      arm_thread_state64_t state;
      mach_msg_type_number_t state_count = ARM_THREAD_STATE64_COUNT;

      kern_return_t kr = thread_get_state(thread, ARM_THREAD_STATE64, (thread_state_t)&state, &state_count);
      if (kr != KERN_SUCCESS) {
          printf("Error getting thread state: %s\n", mach_error_string(kr));
      }
      if (exceptionCallback) {
        exceptionCallback(state, [this, exceptionMessage, state, state_count]() {
              continueFromBreak(exceptionMessage, state, state_count);
          });
      } else {
        continueFromBreak(exceptionMessage, state, state_count);
      }
    } else {
        os_log(OS_LOG_DEFAULT, "Not breakpoint message");
    }
  }

  return nullptr;
}

void SimpleDebugger::continueFromBreak(MachExceptionMessage exceptionMessage, arm_thread_state64_t state, mach_msg_type_number_t state_count) {

  if (originalInstruction.contains(state.__pc)) {
    uint32_t orig = originalInstruction.at(state.__pc);
    setInstruction(state.__pc, orig);
    originalInstruction.erase(state.__pc);
  } else {
    // Address was not tracked, do nothing. Maybe this was a thread that hit the breakpoint
    // at the same time another thread cleared it.
    printf("Unexpected not tracked address\n");
  }

  MachReplyMessage replyMessage = {{0}};

  replyMessage.header = exceptionMessage.header;
  replyMessage.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(exceptionMessage.header.msgh_bits), 0);
  replyMessage.header.msgh_local_port = MACH_PORT_NULL;
  replyMessage.header.msgh_size = sizeof(replyMessage);
  replyMessage.NDR = exceptionMessage.NDR;
  replyMessage.returnCode = KERN_SUCCESS;
  replyMessage.header.msgh_id = exceptionMessage.header.msgh_id + 100;

  kern_return_t kr = mach_msg(&replyMessage.header,
      MACH_SEND_MSG,
      sizeof(replyMessage),
      0,
      MACH_PORT_NULL,
      MACH_MSG_TIMEOUT_NONE,
      MACH_PORT_NULL);

  if (kr != KERN_SUCCESS) {
    os_log(OS_LOG_DEFAULT, "Error sending reply: %s", mach_error_string(kr));
  }
}

#endif
