//
//  PLCCPPExceptionHandler.mm
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 12/2/20.
//

#include "PLCrashAsyncThread.h"
#include "PLCCPPExceptionHandler.h"
#include <cxxabi.h>
#include <exception>
#include <string>
#include <typeinfo>
#include <dlfcn.h>
#include "PLCrashMacros.h"
#include <pthread.h>
#include "PLCrashFrameWalker.h"
#include <inttypes.h>
#include "PLCrashAsyncSymbolication.h"
#include "PLCrashAsyncImageList.h"


//static plcrash_log_writer_t *writer;
//static bool calledOnce = false;
//plcrash_async_thread_state_t pl_cpp_thread_state;
plcrash_async_thread_state_t pl_cpp_thread_state_final;

static std::terminate_handler originalHandler;

struct cpp_exception_callback_live_cb_ctx {
    int crashed_thread;
};

static plcrash_error_t plcr_cpp_exception_callback(plcrash_async_thread_state_t *state, void *ctx) {
//    PLCF_DEBUG("plcr_cpp_exception_callback");
    if (state == NULL) {
//        PLCF_DEBUG("state is null");
    }
    pl_cpp_thread_state_final = *state;
//    PLCF_DEBUG("ended plcr_cpp_exception_callback");
    return PLCRASH_ESUCCESS;
}

//static void plcrash_cpp_symbol_cb (pl_vm_address_t address, const char *name, void *ctx) {
//    PLCF_DEBUG("Frame: %s", name);
//}
typedef void (*cxa_throw_type)(void*, std::type_info*, void (*)(void*));

extern "C"
{
    void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*)) __attribute__ ((weak));

    void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*))
    {
//        PLCF_DEBUG("Caught C++ exception");
//        PLCF_DEBUG("Getting current thread state");
        struct cpp_exception_callback_live_cb_ctx live_ctx = {
            .crashed_thread = 1,
        };
//        if (calledOnce == false) {
            plcrash_async_thread_state_current(plcr_cpp_exception_callback, &live_ctx);
//        pl_cpp_thread_state.cursor = &cursor;
//        plcrash_async_memset(&pl_cpp_thread_state_final, 0, sizeof(pl_cpp_thread_state_final));
////        pl_cpp_thread_state_final.stack_direction = pl_cpp_thread_state.stack_direction;
////        pl_cpp_thread_state_final.greg_size = pl_cpp_thread_state.greg_size;
////        pl_cpp_thread_state_final.valid_regs = pl_cpp_thread_state.valid_regs;
//
//#ifdef PLCRASH_ASYNC_THREAD_ARM_SUPPORT
//        PLCF_DEBUG("Ran arm64 code");
////        plcrash_async_memcpy(&pl_cpp_thread_state_final, &pl_cpp_thread_state, sizeof(pl_cpp_thread_state_final));
////        newOne.arm_state = pl_cpp_thread_state.arm_state;
//#endif
//#ifdef PLCRASH_ASYNC_THREAD_X86_SUPPORT
//        pl_cpp_thread_state_final.x86_state = pl_cpp_thread_state.x86_state;
//#endif
        plcrash_greg_t pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
        PLCF_DEBUG("PC at __cxa_throw: 0x%" PRIx64, (uint64_t) pc);
//        pl_cpp_thread_state = newOne;
//        plcrash_async_state_fr
        
        plcrash_async_symbol_cache_t findContext;
        plcrash_error_t err = plcrash_async_symbol_cache_init(&findContext);
        /* Abort if it failed, although that should never actually happen, ever. */
        if (err != PLCRASH_ESUCCESS) {
            PLCF_DEBUG("Couldn't create error: %s", plcrash_async_strerror(err));
        }
        PLCF_DEBUG("Creating cursor to loop on all symbols");
        plframe_cursor_t cursor;
        plframe_error_t ferr = plframe_cursor_init(&cursor, mach_task_self(), &pl_cpp_thread_state_final, &shared_image_list);
        if (ferr != PLFRAME_ESUCCESS) {
            PLCF_DEBUG("An error occured initializing the frame cursor: %s", plframe_strerror(ferr));
        }
        plframe_cursor_start_recording(&cursor);
        while ((ferr = plframe_cursor_next(&cursor)) == PLFRAME_ESUCCESS) {
            /* Fetch the PC value */
            plcrash_greg_t pc = 0;
            if ((ferr = plframe_cursor_get_reg(&cursor, PLCRASH_REG_IP, &pc)) != PLFRAME_ESUCCESS) {
                PLCF_DEBUG("Could not retrieve frame PC register: %s", plframe_strerror(ferr));
                break;
            } else {
                PLCF_DEBUG("Next PC loaded: 0x%" PRIx64, (uint64_t) pc);
//                plcrash_greg_t fp = 0;
//                plframe_cursor_get_reg(&cursor, PLCRASH_REG_FP, &fp);
//                plcrash_greg_t sp = 0;
//                plframe_cursor_get_reg(&cursor, PLCRASH_REG_SP, &sp);
//                PLCF_DEBUG("Next FP loaded: 0x%" PRIx64, (uint64_t) fp);
//                PLCF_DEBUG("Next SP loaded: 0x%" PRIx64, (uint64_t) sp);
            }
            plframe_cursor_record(&cursor, cursor.frame.thread_state);

//            plcrash_async_image_list_set_reading(&shared_image_list, true);
//            plcrash_async_image_t *image = plcrash_async_image_containing_address(&shared_image_list, (pl_vm_address_t) pc);
//
//            if (image != NULL) {
//                plcrash_error_t ret;
//                ret = plcrash_async_find_symbol(&image->macho_image, PLCRASH_ASYNC_SYMBOL_STRATEGY_ALL, &findContext, (pl_vm_address_t) pc, plcrash_cpp_symbol_cb, &ret);
//                if (ret != PLCRASH_ESUCCESS) {
//                    PLCF_DEBUG("Failed to get symbol: 0x%" PRIx64, (uint64_t) pc);
//                }
//            } else {
//                PLCF_DEBUG("Failed to get image of symbol: 0x%" PRIx64, (uint64_t) pc);
//            }
//            plcrash_async_image_list_set_reading(&shared_image_list, false);
        }
        
        plframe_cursor_restart_recording(&cursor);
        pl_cpp_thread_state_final.cursor = &cursor;
        PLCF_DEBUG("finished looping on all symbols");

        /* Did we reach the end successfully? */
//        if (ferr != PLFRAME_ENOFRAME) {
//            /* This is non-fatal, and in some circumstances -could- be caused by reaching the end of the stack if the
//             * final frame pointer is not NULL. */
//            PLCF_DEBUG("Terminated stack walking early: %s", plframe_strerror(ferr));
//        }
//            if (err != PLCRASH_ESUCCESS) {
//            }
//            calledOnce = true;
//        } else {
////            PLCF_DEBUG("skipped setting state");
//        }
//        PLCF_DEBUG("Done getting current thread state");
//        PLCF_DEBUG("Get existing thread");
//        thread_act_array_t threads;
//        mach_msg_type_number_t thread_count;
//        /* Get a list of all threads */
//        if (task_threads(mach_task_self(), &threads, &thread_count) != KERN_SUCCESS) {
//            PLCF_DEBUG("Fetching thread list failed");
//            thread_count = 0;
//        }
//        thread_t thr;
//        for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
//            if (threads[i] == pl_mach_thread_self()) {
//                thr = threads[i];
//                break;
//            }
//        }
//
//        PLCF_DEBUG("Create new thread");
//        pthread_t thread;
//        pthread_attr_t attr;
//        void *status;
//        // Initialize and set thread joinable
//        pthread_attr_init(&attr);
//        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
//
//        PLCF_DEBUG("Make thread executable");
//        int rc = pthread_create(&thread, &attr, initializeThreadContext, (void *)&thr);
//        if (rc) {
//            PLCF_DEBUG("couldn't create thread");
//        }
//        PLCF_DEBUG("Thread made executable successfully");
//        PLCF_DEBUG("make current thread waiting for new thread");
//        pthread_join(thread, &status);
//        PLCF_DEBUG("continuing exeuction");

        /* Using NSThread to get stacktrace */
//        NSArray *stackTrace = [NSThread callStackSymbols];
//        PLCF_DEBUG("YH: C++ Crash detected, call stack: ")
//        for (NSString *frame in stackTrace) {
//            PLCF_DEBUG("YH: Frame: %s", [frame UTF8String])
//        }

//        if(g_captureNextStackTrace)
//        {
//            kssc_initSelfThread(&g_stackCursor, 1);
//        }
//#if defined(PLCRASH_ASYNC_THREAD_ARM_SUPPORT) && defined(__LP64__)
//    plcrash_async_thread_state_init(cpp_thread_state, CPU_TYPE_ARM64);
//
//#elif defined(PLCRASH_ASYNC_THREAD_ARM_SUPPORT)
//    plcrash_async_thread_state_init(cpp_thread_state, CPU_TYPE_ARM);
//
//#elif defined(PLCRASH_ASYNC_THREAD_X86_SUPPORT) && defined(__LP64__)
//    plcrash_async_thread_state_init(cpp_thread_state, CPU_TYPE_X86_64);
//
//#elif defined(PLCRASH_ASYNC_THREAD_X86_SUPPORT)
//    plcrash_async_thread_state_init(cpp_thread_state, CPU_TYPE_X86);
//
//#else
//#error Add platform support
//#endif
        
        static cxa_throw_type orig_cxa_throw = NULL;
        if(orig_cxa_throw == NULL)
        {
            orig_cxa_throw = (cxa_throw_type) dlsym(RTLD_NEXT, "__cxa_throw");
        }
        pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
        PLCF_DEBUG("PC at before calling original thrower __cxa_throw: 0x%" PRIx64, (uint64_t) pc);
        orig_cxa_throw(thrown_exception, tinfo, dest);
        pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
        PLCF_DEBUG("PC at after calling original thrower __cxa_throw: 0x%" PRIx64, (uint64_t) pc);
        __builtin_unreachable();
    }
}

//static void PLCExceptionRecord(const char *name,
//                        const char *reason) {
//    PLCF_DEBUG("YH: C++ Not exception with name: %s, reason :%s", name, reason)
//    char* p = strdup("C++ crash");
//    char* pc = strdup("C++ crash reason");
//    writer->uncaught_exception.name = p;
//    writer->uncaught_exception.reason = pc;
//}

//static const char *PLCExceptionDemangle(const char *symbol) {
//    int status;
//    char *buffer = NULL;
//
//    buffer = __cxxabiv1::__cxa_demangle(symbol, buffer, NULL, &status);
//    if (!buffer) {
//      return nil;
//    }
//
//    NSString *result = [NSString stringWithUTF8String:buffer];
//
//    free(buffer);
//
//    return [result UTF8String];
//}

static void PLCCPPTerminateHandler(void) {
    /* Using NSThread to get stacktrace */
//    NSArray *stackTrace = [NSThread callStackSymbols];
//    PLCF_DEBUG("YH: C++ Crash detected, call stack: ")
//    for (NSString *frame in stackTrace) {
//        PLCF_DEBUG("YH: Frame: %s", [frame UTF8String])
//    }

    /* Suspend enviroment */
    thread_act_array_t threads;
    mach_msg_type_number_t thread_count;

    plcrash_greg_t pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
    PLCF_DEBUG("PC at terminate handler: 0x%" PRIx64, (uint64_t) pc);
    /* Get a list of all threads */
    if (task_threads(mach_task_self(), &threads, &thread_count) != KERN_SUCCESS) {
        PLCF_DEBUG("Fetching thread list failed");
        thread_count = 0;
    }
    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        if (threads[i] != pl_mach_thread_self())
            thread_suspend(threads[i]);
    }
    
    pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
    PLCF_DEBUG("PC at terminate handler after suspend: 0x%" PRIx64, (uint64_t) pc);

//    void (*handler)(void) = originalHandler;
//    if (handler == PLCCPPTerminateHandler) {
//        PLCF_DEBUG("YH: Error: original handler was set recursively");
//      handler = NULL;
//    }
//
//    // Restore pre-existing handler, if any. Do this early, so that
//    // if std::terminate is called while we are executing here, we do not recurse.
//    if (handler) {
//        PLCF_DEBUG("YH: restoring pre-existing handler");
//
//      // To prevent infinite recursion in this function, check that we aren't resetting the terminate
//      // handler to the same function again, which would be this function in the event that we can't
//      // actually change the handler during a terminate.
//      if (std::set_terminate(handler) == handler) {
//        PLCF_DEBUG("YH: handler has already been restored, aborting");
//        abort();
//      }
//    }

    const char* name = NULL;
    std::type_info* tinfo = __cxxabiv1::__cxa_current_exception_type();
    if(tinfo != NULL)
    {
        name = tinfo->name();
    }
    if(name == NULL || strcmp(name, "NSException") != 0) {
        PLCF_DEBUG("Rethrow exception");
//        try
//        {
//            throw;
//        }
//        catch(std::exception& exc)
//        {
//            PLCF_DEBUG("caught exception");
//    //        strncpy(descriptionBuff, exc.what(), sizeof(descriptionBuff));
//        }
    } else {
        PLCF_DEBUG("Captured NSException, let NSException handler handle it ");
    }
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
////        PLCExceptionRecordNSException(exception);
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
////        PLCExceptionRecord(PLCExceptionDemangle(name), exc.what());
//    } catch (const std::exception *exc) {
////        PLCExceptionRecord(PLCExceptionDemangle(name), exc->what());
//    } catch (const std::bad_alloc &exc) {
//        // it is especially important to avoid demangling in this case, because the expetation at this
//        // point is that all allocations could fail
//        PLCExceptionRecord("std::bad_alloc", exc.what());
//    } catch (...) {
////        PLCExceptionRecord(PLCExceptionDemangle(name), "");
//    }
    
    // only do this if there was a pre-existing handler
//    if (handler) {
//        PLCF_DEBUG("YH: invoking pre-existing handler");
//      handler();
//    }
    
    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        if (threads[i] != pl_mach_thread_self())
            thread_resume(threads[i]);
    }
    
    pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
    PLCF_DEBUG("PC at terminate handler after resume: 0x%" PRIx64, (uint64_t) pc);
//    abort();
    PLCF_DEBUG("YH: calling original handler")
    
    pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
    PLCF_DEBUG("PC at terminate handler before calling original handler: 0x%" PRIx64, (uint64_t) pc);
    if (originalHandler != NULL) {
        originalHandler();
    } else {
        PLCF_DEBUG("YH: original handler is nil")
    }
    pc = plcrash_async_thread_state_get_reg(&pl_cpp_thread_state_final, PLCRASH_REG_IP);
    PLCF_DEBUG("PC at terminate handler after calling original handler: 0x%" PRIx64, (uint64_t) pc);
//    originalHandler();
}

void setCPPExceptionHandler() {
    originalHandler = std::set_terminate(PLCCPPTerminateHandler);
    PLCF_DEBUG("YH: did set terminate method")
//    writer = targetWriter;
}
