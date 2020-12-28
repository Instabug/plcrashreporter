//
//  PLCCPPExceptionHandler.mm
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 12/2/20.
//

#include "PLCrashReporter.h"
#include "PLCCPPExceptionHandler.h"
#include <cxxabi.h>
#include <exception>
#include <string>
#include <typeinfo>

static plcrash_log_writer_t *writer;
static void (*originalHandler)(void);

//static void PLCExceptionRecordNSException(NSException *exception) {
//    plcrash_log_writer_set_exception(writer, exception);
//}

static void PLCExceptionRecord(const char *name,
                        const char *reason) {
                PLCF_DEBUG("YH: C++ Not exception with name: %s, reason :%s", name, reason)
    char* p = strdup("C++ crash");
    char* pc = strdup("C++ crash reason");
    writer->uncaught_exception.name = p;
    writer->uncaught_exception.reason = pc;
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
    /* Using NSThread to get stacktrace */
//    NSArray *stackTrace = [NSThread callStackSymbols];
//    PLCF_DEBUG("YH: C++ Crash detected, call stack: ")
//    for (NSString *frame in stackTrace) {
//        PLCF_DEBUG("YH: Frame: %s", [frame UTF8String])
//    }
    void (*handler)(void) = originalHandler;
    if (handler == PLCCPPTerminateHandler) {
        PLCF_DEBUG("YH: Error: original handler was set recursively");
      handler = NULL;
    }

    // Restore pre-existing handler, if any. Do this early, so that
    // if std::terminate is called while we are executing here, we do not recurse.
    if (handler) {
        PLCF_DEBUG("YH: restoring pre-existing handler");

      // To prevent infinite recursion in this function, check that we aren't resetting the terminate
      // handler to the same function again, which would be this function in the event that we can't
      // actually change the handler during a terminate.
      if (std::set_terminate(handler) == handler) {
        PLCF_DEBUG("YH: handler has already been restored, aborting");
        abort();
      }
    }

    std::type_info *typeInfo = __cxxabiv1::__cxa_current_exception_type();
    if (typeInfo) {
        const char *name = typeInfo->name();
        PLCExceptionRecord(PLCExceptionDemangle(name), "");
    } else {
        PLCF_DEBUG("YH: no active exception");
    }
//    PLCF_DEBUG("YH: C++ Crash detected name: %s", name)
//    try {
//    @try {
//        // This could potentially cause a call to std::terminate() if there is actually no active
//        // exception.
//
//        PLCF_DEBUG("YH: Throwing excpetion")
//        throw;
//    } @catch (NSException *exception) {
//    //    #if TARGET_OS_IPHONE
//        PLCF_DEBUG("YH: C++ Crash detected as exception")
//        PLCExceptionRecordNSException(exception);
//
//    //    #else
//    //          // There's no need to record this here, because we're going to get
//    //          // the value forward to us by AppKit
//    //          FIRCLSSDKLog("Skipping ObjC exception at this point");
//    //    #endif
//    }
//    } catch (const char *exc) {
//        PLCExceptionRecord("const char *", exc);
//    } catch (const std::string &exc) {
//        PLCExceptionRecord("std::string", exc.c_str());
//    } catch (const std::exception &exc) {
//        PLCExceptionRecord(PLCExceptionDemangle(name), exc.what());
//    } catch (const std::exception *exc) {
//        PLCExceptionRecord(PLCExceptionDemangle(name), exc->what());
//    } catch (const std::bad_alloc &exc) {
//        // it is especially important to avoid demangling in this case, because the expetation at this
//        // point is that all allocations could fail
//        PLCExceptionRecord("std::bad_alloc", exc.what());
//    } catch (...) {
//        PLCExceptionRecord(PLCExceptionDemangle(name), "");
//    }
    
    // only do this if there was a pre-existing handler
    if (handler) {
        PLCF_DEBUG("YH: invoking pre-existing handler");
      handler();
    }
    PLCF_DEBUG("YH: Aborting")
    abort();
}

void setCPPExceptionHandler(plcrash_log_writer_t *targetWriter) {
    originalHandler = std::set_terminate(PLCCPPTerminateHandler);
    PLCF_DEBUG("YH: did set terminate method")
    writer = targetWriter;
}
