//
//  PLCCPPExceptionHandler.m
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 12/2/20.
//

#import "PLCrashReporter.h"
#import "PLCCPPExceptionHandler.h"
#include <cxxabi.h>
#include <exception>
#include <string>
#include <typeinfo>

static plcrash_log_writer_t *writer;

static void PLCExceptionRecordNSException(NSException *exception) {
    plcrash_log_writer_set_exception(writer, exception);
}

static void PLCExceptionRecord(const char *name,
                        const char *reason) {
                PLCF_DEBUG("YH: C++ Not exception with name: %s, reason :%s", name, reason)
    writer->uncaught_exception.name = strdup(name);
    writer->uncaught_exception.reason = strdup(reason);
}

static const char *PLCExceptionDemangle(const char *symbol) {
    int status;
    char *buffer = NULL;

    buffer = __cxxabiv1::__cxa_demangle(symbol, buffer, NULL, &status);
    if (!buffer) {
      return nil;
    }

    NSString *result = [NSString stringWithUTF8String:buffer];

    free(buffer);

    return [result UTF8String];
}

static void PLCCPPTerminateHandler(void) {
    PLCF_DEBUG("YH: C++ Crash detected")
    std::type_info *typeInfo = __cxxabiv1::__cxa_current_exception_type();
    const char *name = typeInfo->name();
    PLCF_DEBUG("YH: C++ Crash detected name: %s", name)
    try {
    @try {
        // This could potentially cause a call to std::terminate() if there is actually no active
        // exception.
        
        PLCF_DEBUG("YH: Throwing excpetion")
        throw;
    } @catch (NSException *exception) {
    //    #if TARGET_OS_IPHONE
        PLCF_DEBUG("YH: C++ Crash detected as exception")
        PLCExceptionRecordNSException(exception);

    //    #else
    //          // There's no need to record this here, because we're going to get
    //          // the value forward to us by AppKit
    //          FIRCLSSDKLog("Skipping ObjC exception at this point\n");
    //    #endif
    }
    } catch (const char *exc) {
        PLCExceptionRecord("const char *", exc);
    } catch (const std::string &exc) {
        PLCExceptionRecord("std::string", exc.c_str());
    } catch (const std::exception &exc) {
        PLCExceptionRecord(PLCExceptionDemangle(name), exc.what());
    } catch (const std::exception *exc) {
        PLCExceptionRecord(PLCExceptionDemangle(name), exc->what());
    } catch (const std::bad_alloc &exc) {
        // it is especially important to avoid demangling in this case, because the expetation at this
        // point is that all allocations could fail
        PLCExceptionRecord("std::bad_alloc", exc.what());
    } catch (...) {
        PLCExceptionRecord(PLCExceptionDemangle(name), "");
    }
    
    PLCF_DEBUG("YH: Aborting")
    abort();
}

void setCPPExceptionHandler(plcrash_log_writer_t *targetWriter) {
    std::set_terminate(PLCCPPTerminateHandler);
    PLCF_DEBUG("YH: did set terminate method")
    writer = targetWriter;
}
