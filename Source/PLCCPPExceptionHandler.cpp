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
#include <inttypes.h>
#include "PLCrashAsyncSymbolication.h"
#include "PLCrashAsyncImageList.h"
#include "PLCxaThrowSwapper.h"

#define DESCRIPTION_BUFFER_LENGTH 100

// Public
bool pl_cpp_has_cursor = false;
plframe_cursor_t pl_cpp_cursor;
plcrash_cpp_exception_t pl_cpp_exception;

// Private
static plcrash_async_thread_state_t pl_cpp_thread_state_final;

static std::terminate_handler originalHandler;

// Dump struct to to pass for plcrash_async_thread_state_current callback.
struct cpp_exception_callback_live_cb_ctx {
    int crashed_thread;
};


/// Demangle CPP symbols.
/// Used for CPP exception name for now, not symbols.
/// @param mangledSymbol symbol to be demangled
extern "C" char* pl_demangleCPP(const char* mangledSymbol)
{
    int status = 0;
    char* demangled = __cxxabiv1::__cxa_demangle(mangledSymbol, NULL, NULL, &status);
    return status == 0 ? demangled : NULL;
}

/// Callback for plcrash_async_thread_state_current
/// @param state state created from plcrash_async_thread_state_current
/// @param ctx context passed to plcrash_async_thread_state_current
static plcrash_error_t plcr_cpp_exception_callback(plcrash_async_thread_state_t *state, void *ctx) {
    if (state == NULL) {
        PLCF_DEBUG("CPP excpetion state callbacks returned nil");
        return PLCRASH_EINVAL;
    }
    pl_cpp_thread_state_final = *state;
    return PLCRASH_ESUCCESS;
}

/* Overidding the original __cxa_throw to the original stacktrace */
typedef void (*cxa_throw_type)(void*, std::type_info*, void (*)(void*));

extern "C"
{
    void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*)) __attribute__ ((weak));

static void captureStackTraceInCursor(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*)) {
    plcrash_async_symbol_cache_t findContext;
    plframe_error_t ferr;
    
    /* Get thread state for current thread to create cursor and save stack trace */
    struct cpp_exception_callback_live_cb_ctx live_ctx = {
        .crashed_thread = 1,
    };
    plcrash_error_t err = plcrash_async_thread_state_current(plcr_cpp_exception_callback, &live_ctx);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Couldn't create thread state: %s", plcrash_async_strerror(err));
        return;
    }
    
    /* Creating cursor */
    err = plcrash_async_symbol_cache_init(&findContext);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Couldn't create cache: %s", plcrash_async_strerror(err));
    }
    ferr = plframe_cursor_init(&pl_cpp_cursor, mach_task_self(), &pl_cpp_thread_state_final, &shared_image_list);
    if (ferr != PLFRAME_ESUCCESS) {
        PLCF_DEBUG("An error occured initializing the frame cursor: %s", plframe_strerror(ferr));
        return;
    }
    
    /* Start recording frames for cursor */
    plframe_cursor_start_recording(&pl_cpp_cursor);
    while ((ferr = plframe_cursor_next(&pl_cpp_cursor)) == PLFRAME_ESUCCESS) {
        /* Fetch the PC value */
        plcrash_greg_t pc = 0;
        if ((ferr = plframe_cursor_get_reg(&pl_cpp_cursor, PLCRASH_REG_IP, &pc)) != PLFRAME_ESUCCESS) {
            PLCF_DEBUG("Could not retrieve frame PC register: %s", plframe_strerror(ferr));
            break;
        }
        /* Record frame in cursor */
        plframe_cursor_record(&pl_cpp_cursor, pl_cpp_cursor.frame.thread_state);
    }
    
    plframe_cursor_restart_recording(&pl_cpp_cursor);
    plframe_cursor_next(&pl_cpp_cursor); // Skip the first frame; our swap.
    plframe_cursor_next(&pl_cpp_cursor); // Skip the first frame; our __cxa_throw.

    pl_cpp_has_cursor = true;
}

void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*))
    {
        captureStackTraceInCursor(NULL, NULL, NULL);

        static cxa_throw_type orig_cxa_throw = NULL;
        if(orig_cxa_throw == NULL)
        {
            orig_cxa_throw = (cxa_throw_type) dlsym(RTLD_NEXT, "__cxa_throw");
        }
        orig_cxa_throw(thrown_exception, tinfo, dest);
        __builtin_unreachable();
    }
}

static void PLCCPPTerminateHandler(void) {
    /* Suspend threads */
    thread_act_array_t threads;
    mach_msg_type_number_t thread_count;

    /* Get a list of all threads */
    if (task_threads(mach_task_self(), &threads, &thread_count) != KERN_SUCCESS) {
        PLCF_DEBUG("Fetching thread list failed");
        thread_count = 0;
    }
    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        if (threads[i] != pl_mach_thread_self())
            thread_suspend(threads[i]);
    }

    /* Trying to get exception info */
    pl_cpp_exception.has_exception = false;
    const char* name = NULL;
    std::type_info* tinfo = __cxxabiv1::__cxa_current_exception_type();
    if(tinfo != NULL)
    {
        name = pl_demangleCPP(tinfo->name());
    }
    if(name == NULL || strcmp(name, "NSException") != 0) {
        char descriptionBuff[DESCRIPTION_BUFFER_LENGTH];
        const char* description = descriptionBuff;
        descriptionBuff[0] = 0;

        /* Rethrowing exception to get exception info */
        try
        {
            throw;
        }
        catch(std::exception& exc)
        {
            strncpy(descriptionBuff, exc.what(), sizeof(descriptionBuff));
        }
#define CATCH_VALUE(TYPE, PRINTFTYPE) \
catch(TYPE value)\
{ \
    snprintf(descriptionBuff, sizeof(descriptionBuff), "%" #PRINTFTYPE, value); \
}
        CATCH_VALUE(char,                 d)
        CATCH_VALUE(short,                d)
        CATCH_VALUE(int,                  d)
        CATCH_VALUE(long,                ld)
        CATCH_VALUE(long long,          lld)
        CATCH_VALUE(unsigned char,        u)
        CATCH_VALUE(unsigned short,       u)
        CATCH_VALUE(unsigned int,         u)
        CATCH_VALUE(unsigned long,       lu)
        CATCH_VALUE(unsigned long long, llu)
        CATCH_VALUE(float,                f)
        CATCH_VALUE(double,               f)
        CATCH_VALUE(long double,         Lf)
        CATCH_VALUE(char*,                s)
        catch(...)
        {
            description = NULL;
        }
        if (name != NULL || description != NULL) {
            pl_cpp_exception.has_exception = true;
            if (name != NULL) {
                pl_cpp_exception.name = strdup(name);
            }
            if (description != NULL) {
                pl_cpp_exception.reason = strdup(description);
            }
        }
    } else {
        PLCF_DEBUG("Captured NSException, let NSException handler handle it ");
    }

    /* Resume threads */
    for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
        if (threads[i] != pl_mach_thread_self())
            thread_resume(threads[i]);
    }

    PLCF_DEBUG("Trying to call original handler")
    if (originalHandler != NULL) {
        originalHandler();
    } else {
        PLCF_DEBUG("Original CPP exception handler is nil")
    }
}

void plcrash_setUncaughtCPPExceptionHandler() {
    /* Setting PLCCPPTerminateHandler as the handler for CPP exceptions to get exception info */
    originalHandler = std::set_terminate(PLCCPPTerminateHandler);
    PLCF_DEBUG("Did set PLCrashReporter as handler for uncaught CPP exceptions")

    plct_swap(captureStackTraceInCursor);
    PLCF_DEBUG("Did dynamically swap __cxa_throw")
}
