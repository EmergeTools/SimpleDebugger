# 🩺 SimpleDebugger

A minimal demonstration of breakpoints in an iOS app debugger. It can be used for function hooking, like a very lightweight Frida. It’s mainly for demonstration/learning purposes. Works on arm64 simulator and device.

# Getting started

Create an instance of SimpleDebugger like so:

```c++
SimpleDebugger *debugger = new SimpleDebugger();
```

## Set breakpoints

Set breakpoints using the `setBreakpoint(vm_address_t address)` method. The provided address must be in the __TEXT/__text section (the memory region containing executable code).

If you set breakpoints without calling `startDebugging` lldb can handle these breakpoints instead, although continuing past a breakpoint will not automatically work in lldb for breakpoints set by SimpleDebugger. You can manually increment the program counter in lldb to continue.

## Respond to breakpoints

Handle a breakpoint being hit using the `setExceptionCallback` method. The provided callback takes two parameters, one is the CPU state and the other is a function that can be called to continue execution on the thread that hit the breakpoint. Call `startDebugging` to begin receiving events.

## Example:

This example creates a debugger and adds one breakpoint.

```c++
#include <SimpleDebugger.h>

void myFunction() { printf("Hello world\n"); }

void breakpointCallback(arm_thread_state64_t state, std::function<void()> sendReply) {
    printf("Got breakpoint with PC: 0x%llx\n", state.__pc);
    sendReply();
}

__attribute__((constructor)) void example(void);
__attribute__((constructor)) void setup() {
  SimpleDebugger *debugger = new SimpleDebugger();
  debugger->setExceptionCallback(breakpointCallback);
  debugger->setBreakpoint((vm_address_t) &myFunction);
  // You must call start debugging to set up the exception server.
  debugger->startDebugging();

  // The breakpoint handler will run before myFunction
  myFunction();
}
```

# How it works

SimpleDebugger overwrites instructions with a break instruction by modifying the vm protection of the memory address to be writeable. The original instruction is stored in a table and written back after the breakpoint is hit. Break instructions are handled with a mach exception server.