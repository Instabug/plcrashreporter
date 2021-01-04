//
//  PLCCPPExceptionHandler.h
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 12/2/20.
//

#ifndef PLC_CPP_EXCEPTION_HANDLER_h
#define PLC_CPP_EXCEPTION_HANDLER_h

#include <stdio.h>
#include "PLCrashFrameWalker.h"
#import "PLCrashLogWriter.h"

extern plframe_cursor_t pl_cpp_cursor;

PLCR_C_BEGIN_DECLS

//void setCPPExceptionHandler(plcrash_log_writer_t *targetWriter);
void setCPPExceptionHandler(void);

PLCR_C_END_DECLS

#endif /* PLC_CPP_EXCEPTION_HANDLER_h */
