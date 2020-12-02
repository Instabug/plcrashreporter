//
//  PLCCPPExceptionHandler.h
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 12/2/20.
//

#import <Foundation/Foundation.h>
#import "PLCrashMacros.h"
#import "PLCrashLogWriter.h"

PLCR_C_BEGIN_DECLS

void setCPPExceptionHandler(plcrash_log_writer_t *targetWriter);

PLCR_C_END_DECLS
