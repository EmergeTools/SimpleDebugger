#import <UIKit/UIKit.h>
#import <pthread.h>
#import <os/log.h>

#define BREAKPOINT_ENABLE 481
#define BREAKPOINT_DISABLE 0

#import <mach/mach.h>

#pragma pack(4)
struct MachExceptionMessage
{
  mach_msg_header_t          header;
  mach_msg_body_t            body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  NDR_record_t               NDR;
  exception_type_t           exception;
  mach_msg_type_number_t     codeCount;
  mach_exception_data_type_t code[0];
  char                       padding[512];
};
#pragma pack()

#pragma pack(4)
struct MachReplyMessage
{
  mach_msg_header_t header;
  NDR_record_t      NDR;
  kern_return_t     returnCode;
};
#pragma pack()


thread_t main_thread;
pthread_t serverThread;
mach_port_t server;

int status = 0;
int hasCalledHandler = false;

UIColor *makeHookedColor(void) {
  return UIColor.greenColor;
}

void * exceptionServerWrapper(void * input) {
  
  struct MachExceptionMessage exceptionMessage = {{0}};

  os_log(OS_LOG_DEFAULT, "Exception server started");

  while (true) {
    kern_return_t kr = mach_msg(&exceptionMessage.header,
                                MACH_RCV_MSG,
                                0,
                                sizeof(exceptionMessage),
                                server,
                                MACH_MSG_TIMEOUT_NONE,
                                MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
      os_log(OS_LOG_DEFAULT, "Error receiving message");
      continue;
    }
    
    mach_port_t thread = exceptionMessage.thread.name;
    printf("The thread %d\n", thread);
    printf("The main thread %d\n", main_thread);
    arm_thread_state64_t state;
    mach_msg_type_number_t state_count = ARM_THREAD_STATE64_COUNT;

    kr = thread_get_state(thread, ARM_THREAD_STATE64, (thread_state_t)&state, &state_count);
    if (kr != KERN_SUCCESS) {
        printf("Error getting thread state: %s\n", mach_error_string(kr));
    }

    if (exceptionMessage.exception == EXC_BREAKPOINT) {
      os_log(OS_LOG_DEFAULT, "Hit breakpoint");
      
      state.__pc = (__uint64_t) &makeHookedColor;
      kr = thread_set_state(thread, ARM_THREAD_STATE64, (thread_state_t)&state, state_count);
      if (kr != KERN_SUCCESS) {
          printf("Error setting thread state: %s\n", mach_error_string(kr));
        status = -3;
      }
      hasCalledHandler = true;
      
      struct MachReplyMessage replyMessage = {{0}};

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
      
    } else {
        os_log(OS_LOG_DEFAULT, "Not breakpoint message");
      printf("msg %d", exceptionMessage.exception);
    }
  }
  
  return 0;
}

kern_return_t start_exception_server(void) {
    kern_return_t kret;

    //allocate mach port with a receive right for our remote task
    kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &server);

    if (kret != KERN_SUCCESS) {
        printf("Could not start exception server with error: %s\n", mach_error_string(kret));
        return KERN_FAILURE;
    }
  
  if (mach_port_insert_right(mach_task_self(), server, server, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
    return false;
  }

    //this makes our exception server an ARM64 exception handler. currently only supporting breakpoints!
    kret = task_set_exception_ports(mach_task_self(), EXC_MASK_BREAKPOINT, server, EXCEPTION_DEFAULT, ARM_THREAD_STATE64);
    if (kret != KERN_SUCCESS) {
        printf("Could not set task exception port with error: %s\n", mach_error_string(kret));
        return KERN_FAILURE;
    }
    pthread_create(&serverThread, nil, &exceptionServerWrapper, nil);
    return KERN_SUCCESS;
}

__attribute__((constructor)) void setup(void);
__attribute__((constructor)) void setup(void) {
  main_thread = mach_thread_self();
  start_exception_server();
}

UIColor* makeColor(void) {
  if (status == -3) {
    return UIColor.redColor;
  }
  if (hasCalledHandler) {
    return UIColor.purpleColor;
  }
  return UIColor.blueColor;
}

// Must not be called on the main thread
int installHook(void) {
  kern_return_t kret;

  arm_debug_state64_t state;
  mach_msg_type_number_t state_count;

  void * address = (void *) &makeColor;
  
  thread_suspend(main_thread);
  
  state_count = ARM_DEBUG_STATE64_COUNT;
  kret = thread_get_state(main_thread, ARM_DEBUG_STATE64, (thread_state_t) &state, &state_count);
  if (kret != KERN_SUCCESS) {
      printf("Could not get thread_get_state with error: %s\n", mach_error_string(kret));
    return -1;
  }
 
  int br_count = 0;
  
  state.__bvr[br_count] = (__uint64_t) address;
  state.__bcr[br_count] = BREAKPOINT_ENABLE;
  br_count++;
  
  kret = thread_set_state(main_thread, ARM_DEBUG_STATE64, (thread_state_t)&state, state_count);
  if (kret != KERN_SUCCESS) {
      printf("Could not get thread_set_state with error: %s\n", mach_error_string(kret));
      return -2;
  }
  
  thread_resume(main_thread);
  
  return 0;
}
