/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "PLCrashFrameWalker.h"
#include "PLCrashAsync.h"

#include "PLCrashFrameStackUnwind.h"
#include "PLCrashFrameCompactUnwind.h"
#include "PLCrashFrameDWARFUnwind.h"

#include "PLCrashFeatureConfig.h"
#include <inttypes.h>
#include <stdlib.h>

#pragma mark Error Handling

/**
 * Return an error description for the given plframe_error_t.
 */
const char *plframe_strerror (plframe_error_t error) {
    switch (error) {
        case PLFRAME_ESUCCESS:
            return "No error";
        case PLFRAME_EUNKNOWN:
            return "Unknown error";
        case PLFRAME_ENOFRAME:
            return "No frames are available";
        case PLFRAME_EBADFRAME:
            return "Corrupted frame";
        case PLFRAME_ENOTSUP:
            return "Operation not supported";
        case PLFRAME_EINVAL:
            return "Invalid argument";
        case PLFRAME_INTERNAL:
            return "Internal error";
        case PLFRAME_EBADREG:
            return "Invalid register";
    }

    /* Should be unreachable */
    return "Unhandled error code";
}

plframe_error_t plframe_cursor_thread_state_recorded_at(struct plframe_cursor *cursor, size_t index, plcrash_async_thread_state_t *thread_state) {
//    cursor->_list->set_reading(true); {
//        plcrash::async::async_list<plframe_cursor_info>::node *next = NULL;
//        for (size_t i = 0; i <= index;i++) {
//            next = cursor->_list->next(next);
//        }
    if (index >= cursor->_max_depth) {
        return PLFRAME_ENOFRAME;
    }
    *thread_state = cursor->_list[index];
//    plcrash_async_thread_state_set_reg(thread_state, PLCRASH_REG_FP, info.fp);
//    plcrash_async_thread_state_set_reg(thread_state, PLCRASH_REG_IP, info.ip);
//    plcrash_async_thread_state_set_reg(thread_state, PLCRASH_REG_SP, info.sp);
//    memset(&thread_state->valid_regs, 0xFF, sizeof(thread_state->valid_regs));
//        if (next != NULL) {
////            *thread_state ;
//            plframe_cursor_info info = next->value();
////            plcrash_async_thread_state_clear_all_regs(thread_state);
//
//    PLCF_DEBUG("Loading at index: %zu", index);
//            PLCF_DEBUG("\tLoading IP with value: 0x%" PRIx64, (uint64_t) info.ip);
//        } else {
//            return PLFRAME_INTERNAL;
//        }
//    } cursor->_list->set_reading(false);
    return PLFRAME_ESUCCESS;
}

#pragma mark Frame Walking

/**
 * @internal
 * Shared initializer. Assumes that the initial frame has all registers available.
 *
 * @param cursor Cursor record to be initialized.
 * @param task The task from which @a uap was derived. All memory will be mapped from this task.
 * @param image_list The task's current image list. This is a borrowed reference, and must remain valid for the lifetime of the cursor.
 */
static void plframe_cursor_internal_init (plframe_cursor_t *cursor, task_t task, plcrash_async_image_list_t *image_list) {
    cursor->depth = 0;
    cursor->task = task;
    cursor->image_list = image_list;
    cursor->_recorded = false;
    mach_port_mod_refs(mach_task_self(), cursor->task, MACH_PORT_RIGHT_SEND, 1);    
}

/**
 * Initialize the frame cursor using the provided thread state.
 *
 * @param cursor Cursor record to be initialized.
 * @param task The task from which @a uap was derived. All memory will be mapped from this task.
 * @param thread_state The thread state to use for cursor initialization.
 * @param image_list The task's current image list. This is a borrowed reference, and must remain valid for the lifetime of the cursor.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 *
 * @warning Callers must call plframe_cursor_free() on @a cursor to free any associated resources, even if initialization
 * fails.
 */
plframe_error_t plframe_cursor_init (plframe_cursor_t *cursor, task_t task, plcrash_async_thread_state_t *thread_state, plcrash_async_image_list_t *image_list) {
    plframe_cursor_internal_init(cursor, task, image_list);

    plcrash_async_memcpy(&cursor->frame.thread_state, thread_state, sizeof(cursor->frame.thread_state));

    return PLFRAME_ESUCCESS;
}

/**
 * Initialize the frame cursor by acquiring state from the provided mach thread. If the thread is not suspended,
 * the fetched state may be inconsistent.
 *
 * @param cursor Cursor record to be initialized.
 * @param task The task in which @a thread is running. All memory will be mapped from this task.
 * @param thread The thread to use for cursor initialization.
 * @param image_list The task's current image list. This is a borrowed reference, and must remain valid for the lifetime of the cursor.
 *
 * @return Returns PLFRAME_ESUCCESS on success, or standard plframe_error_t code if an error occurs.
 *
 * @warning Callers must call plframe_cursor_free() on @a cursor to free any associated resources, even if initialization
 * fails.
 */
plframe_error_t plframe_cursor_thread_init (plframe_cursor_t *cursor, task_t task, thread_t thread, plcrash_async_image_list_t *image_list) {
    /* Standard initialization */
    plframe_cursor_internal_init(cursor, task, image_list);
    
    return (plframe_error_t)plcrash_async_thread_state_mach_thread_init(&cursor->frame.thread_state, thread);
}

/**
 * Fetch the next frame using the provided frame readers.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @param readers Frame readers to be used to fetch the next frame. Each reader will be executed in the provided order until a valid frame is read.
 * @param reader_count The number of readers provided in @a readers.
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_next_with_readers (plframe_cursor_t *cursor, plframe_cursor_frame_reader_t *readers[], size_t reader_count) {
    PLCF_DEBUG("Loading next frame with cursor (%p) info: %d %d %d", cursor, cursor->_recorded, cursor->depth, cursor->_max_depth);
    /* The first frame is already available via existing thread state. */
    if (cursor->depth == 0) {
        cursor->depth++;
        if (cursor->_recorded) {
            plframe_error_t err = plframe_cursor_thread_state_recorded_at(cursor, 0, &cursor->frame.thread_state);
            if (err != PLFRAME_ESUCCESS) {
                PLCF_DEBUG("No state has been recoreded: %s", IBGplframe_strerror(err));
                return PLFRAME_EUNKNOWN;
            }
        }
        return PLFRAME_ESUCCESS;
    }
    
    /* A previous frame is only available if we're on the second frame */
    plframe_stackframe_t *prev_frame = NULL;
    if (cursor->depth >= 2)
        prev_frame = &cursor->prev_frame;
    
    /* Read in the next frame using the first successful frame reader. */
    plframe_stackframe_t frame;
    plframe_error_t ferr = PLFRAME_EINVAL; // default return value if reader_count is 0.
    
    if (cursor->_recorded) {
        PLCF_DEBUG("Loading from recorded frames");
        plcrash_async_thread_state_t thread_state;
        ferr = plframe_cursor_thread_state_recorded_at(cursor, cursor->depth, &thread_state);
        frame.thread_state = thread_state;
    } else {
        PLCF_DEBUG("Reading new frames");
        for (size_t i = 0; i < reader_count; i++) {
            ferr = readers[i](cursor->task, cursor->image_list, &cursor->frame, prev_frame, &frame);
            if (ferr == PLFRAME_ESUCCESS)
                break;
        }
    }
    
    if (ferr != PLFRAME_ESUCCESS) {
        return ferr;
    }

    /* Check for completion */
    if (!plcrash_async_thread_state_has_reg(&frame.thread_state, PLCRASH_REG_IP)) {
        PLCF_DEBUG("Missing expected IP value in successfully read frame");
        return PLFRAME_ENOFRAME;
    }
    
    /* A pc within the NULL page is a terminating frame */
    plcrash_greg_t ip = plcrash_async_thread_state_get_reg(&frame.thread_state, PLCRASH_REG_IP);
    if (ip <= PAGE_SIZE)
        return PLFRAME_ENOFRAME;
    
    /* Save the newly fetched frame */
    cursor->prev_frame = cursor->frame;
    cursor->frame = frame;
    cursor->depth++;
    
    return PLFRAME_ESUCCESS;
}

/**
 * Fetch the next frame.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_next (plframe_cursor_t *cursor) {
    plframe_cursor_frame_reader_t *readers[] = {

#if PLCRASH_FEATURE_UNWIND_COMPACT
        plframe_cursor_read_compact_unwind,
#endif

#if PLCRASH_FEATURE_UNWIND_DWARF
        plframe_cursor_read_dwarf_unwind,
#endif

        plframe_cursor_read_frame_ptr
    };

    return plframe_cursor_next_with_readers(cursor, readers, sizeof(readers)/sizeof(readers[0]));
}


/**
 * Get a register value. Returns PLFRAME_ENOTSUP if the given register is unavailable within the current frame.
 *
 * @param cursor A cursor instance representing a valid frame, as initialized by plframe_cursor_next().
 * @param regnum The register to fetch from the current frame's state.
 * @param reg On success, will be set to the register's value.
 */
plframe_error_t plframe_cursor_get_reg (plframe_cursor_t *cursor, plcrash_regnum_t regnum, plcrash_greg_t *reg) {
    /* Verify that the register is available */
    if (!plcrash_async_thread_state_has_reg(&cursor->frame.thread_state, regnum))
        return PLFRAME_ENOTSUP;

    /* Fetch from thread state */
    *reg = plcrash_async_thread_state_get_reg(&cursor->frame.thread_state, regnum);
    return PLFRAME_ESUCCESS;
}

/**
 * Get a register's name.
 *
 * @param cursor A cursor instance initialized with plframe_cursor_init();
 * @param regnum The register number for which a name should be returned.
 */
char const *plframe_cursor_get_regname (plframe_cursor_t *cursor, plcrash_regnum_t regnum) {
    return plcrash_async_thread_state_get_reg_name(&cursor->frame.thread_state, regnum);
}

/**
 * Get the total number of registers supported by the @a cursor's target thread.
 *
 * @param cursor The target cursor.
 */
size_t plframe_cursor_get_regcount (plframe_cursor_t *cursor) {
    return plcrash_async_thread_state_get_reg_count(&cursor->frame.thread_state);
}

void plframe_cursor_record (plframe_cursor_t *cursor, plcrash_async_thread_state_t thread_state) {
//    if (cursor->_list == NULL) {
//        PLCF_DEBUG("Trying to record in a cursor before _list is initialized, recording is canceled.");
//        PLCF_ASSERT(false);
//        return;
//    }
    struct plframe_cursor_info frame_info = {
        .ip = plcrash_async_thread_state_get_reg(&thread_state, PLCRASH_REG_IP),
        .sp = plcrash_async_thread_state_get_reg(&thread_state, PLCRASH_REG_SP),
        .fp = plcrash_async_thread_state_get_reg(&thread_state, PLCRASH_REG_FP)
    };
    PLCF_DEBUG("Recording at index: %d", cursor->depth - 1);
    PLCF_DEBUG("\tRecording IP with value: 0x%" PRIx64, (uint64_t) frame_info.ip);
//    frame_info.pc = plcrash_async_thread_state_get_reg(&thread_state, PL)
    cursor->_list[cursor->depth - 1] = thread_state;
}

void plframe_cursor_start_recording (plframe_cursor_t *cursor) {
//    cursor->_list = new plcrash::async::async_list<plframe_cursor_info>();
    cursor->_list = (plcrash_async_thread_state_t *) malloc(250 * sizeof(plcrash_async_thread_state_t));
}

void plframe_cursor_restart_recording (plframe_cursor_t *cursor) {
    cursor->_recorded = true;
    cursor->_max_depth = cursor->depth;
    cursor->depth = 0;
}

/**
 * Free any resources associated with the frame cursor.
 *
 * @param cursor Cursor record to be freed
 */
void plframe_cursor_free(plframe_cursor_t *cursor) {
    if (cursor->task != MACH_PORT_NULL)
        mach_port_mod_refs(mach_task_self(), cursor->task, MACH_PORT_RIGHT_SEND, -1);
}
