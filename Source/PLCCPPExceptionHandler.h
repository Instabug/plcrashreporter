//
//  PLCCPPExceptionHandler.h
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 12/2/20.
//

#ifndef PLC_CPP_EXCEPTION_HANDLER_h
#define PLC_CPP_EXCEPTION_HANDLER_h

#include <stdio.h>
#include "PLCrashMacros.h"
#include "PLCrashFrameWalker.h"

PLCR_C_BEGIN_DECLS

typedef struct plcrash_cpp_exception {
    bool has_exception;
    char *name;
    char *reason;
} plcrash_cpp_exception_t;

extern plframe_cursor_t pl_cpp_cursor;
extern plcrash_cpp_exception_t pl_cpp_exception;

void setCPPExceptionHandler(void);

PLCR_C_END_DECLS

#endif /* PLC_CPP_EXCEPTION_HANDLER_h */
