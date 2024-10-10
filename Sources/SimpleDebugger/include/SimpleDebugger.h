//
//  SimpleDebugger.h
//  SimpleDebugger
//
//  Created by Noah Martin on 10/9/24.
//

#if defined(__arm64__) || defined(__aarch64__)

#ifdef __cplusplus
extern "C++" {

#import <functional>
#import <mach/mach.h>
#import <pthread.h>
#import <mutex>
#import <unordered_map>


struct MachExceptionMessage;

class SimpleDebugger {
public:
  using ExceptionCallback = std::function<void(arm_thread_state64_t state, std::function<void()>)>;

  SimpleDebugger();

  bool startDebugging();
  void setExceptionCallback(ExceptionCallback callback);
  void setBreakpoint(vm_address_t address);

  ~SimpleDebugger();

private:
  mach_port_t exceptionPort;
  pthread_t serverThread;
  std::mutex m;
  ExceptionCallback exceptionCallback;
  std::unordered_map<vm_address_t, uint32_t> originalInstruction;

  static void* exceptionServerWrapper(void* arg);
  void* exceptionServer();
  void continueFromBreak(MachExceptionMessage exceptionMessage, arm_thread_state64_t state, mach_msg_type_number_t state_count);
};
}
#endif

#endif
