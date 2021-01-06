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
    /** Flag specifying wether an uncaught exception is available. */
    bool has_exception;

    /** Exception name (nullable) e.g. "std::out_of_range" */
    char *name;

    /** Exception reason (nullable) e.g. "vector" */
    char *reason;
} plcrash_cpp_exception_t;

/// Flags wether the value of `pl_cpp_cursor` is available.
extern bool pl_cpp_has_cursor;

/// Carries the cursor created during the CPP exception to be used to retrieve the stacktrace.
extern plframe_cursor_t pl_cpp_cursor;

/// Carries the caught CPP exception information.
extern plcrash_cpp_exception_t pl_cpp_exception;

/// Sets uncaught CPP exception handler.
void plcrash_setUncaughtCPPExceptionHandler(void);

PLCR_C_END_DECLS

#endif /* PLC_CPP_EXCEPTION_HANDLER_h */
