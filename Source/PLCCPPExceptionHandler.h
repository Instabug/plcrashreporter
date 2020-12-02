//
//  PLCCPPExceptionHandler.h
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 12/2/20.
//

#import <Foundation/Foundation.h>
#import "PLCrashLogWriter.h"

void setCPPExceptionHandler(plcrash_log_writer_t *targetWriter);
