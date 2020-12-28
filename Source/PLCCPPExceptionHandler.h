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
#include "PLCrashLogWriter.h"

PLCR_C_BEGIN_DECLS

void setCPPExceptionHandler(plcrash_log_writer_t *targetWriter);

PLCR_C_END_DECLS

#endif /* PLC_CPP_EXCEPTION_HANDLER_h */
