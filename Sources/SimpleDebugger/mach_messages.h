//
//  mach_messages.h
//  SimpleDebugger
//
//  Created by Noah Martin on 10/9/24.
//

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
