/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
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

#import "CrashReporter.h"
#import "PLCrashReport.h"
#import "PLCrashReport.pb-c.h"
#import "PLCrashAsyncThread.h"

struct _PLCrashReportDecoder {
    Plcrash__CrashReport *crashReport;
};

@interface PLCrashReport (PrivateMethods)

- (Plcrash__CrashReport *) decodeCrashData: (NSData *) data error: (NSError **) outError;
- (PLCrashReportSystemInfo *) extractSystemInfo: (Plcrash__CrashReport__SystemInfo *) systemInfo
                                  processorInfo: (PLCrashReportProcessorInfo *) processorInfo
                                          error: (NSError **) outError;
- (PLCrashReportProcessorInfo *) synthesizeProcessorInfoFromArchitecture: (Plcrash__Architecture) architecture error: (NSError **) outError;
- (PLCrashReportProcessorInfo *) extractProcessorInfo: (Plcrash__CrashReport__Processor *) processorInfo error: (NSError **) outError;
- (PLCrashReportMachineInfo *) extractMachineInfo: (Plcrash__CrashReport__MachineInfo *) machineInfo error: (NSError **) outError;
- (PLCrashReportApplicationInfo *) extractApplicationInfo: (Plcrash__CrashReport__ApplicationInfo *) applicationInfo error: (NSError **) outError;
- (PLCrashReportProcessInfo *) extractProcessInfo: (Plcrash__CrashReport__ProcessInfo *) processInfo error: (NSError **) outError;
- (NSArray *) extractThreadInfo: (Plcrash__CrashReport *) crashReport error: (NSError **) outError;
- (NSArray *) extractImageInfo: (Plcrash__CrashReport *) crashReport error: (NSError **) outError;
- (PLCrashReportExceptionInfo *) extractExceptionInfo: (Plcrash__CrashReport__Exception *) exceptionInfo error: (NSError **) outError;
- (PLCrashReportSignalInfo *) extractSignalInfo: (Plcrash__CrashReport__Signal *) signalInfo error: (NSError **) outError;
- (PLCrashReportMachExceptionInfo *) extractMachExceptionInfo: (Plcrash__CrashReport__Signal__MachException *) machExceptionInfo error: (NSError **) outError;

@end


static void populate_nserror (NSError **error, PLCrashReporterError code, NSString *description);

/**
 * Provides decoding of crash logs generated by the PLCrashReporter framework.
 *
 * @warning This API should be considered in-development and subject to change.
 */
@implementation PLCrashReport

/**
 * Initialize with the provided crash log data. On error, nil will be returned, and
 * an NSError instance will be provided via @a error, if non-NULL.
 *
 * @param encodedData Encoded plcrash crash log.
 * @param outError If an error occurs, this pointer will contain an NSError object
 * indicating why the crash log could not be parsed. If no error occurs, this parameter
 * will be left unmodified. You may specify NULL for this parameter, and no error information
 * will be provided.
 *
 * @par Designated Initializer
 * This method is the designated initializer for the PLCrashReport class.
 */
- (id) initWithData: (NSData *) encodedData error: (NSError **) outError {
    if ((self = [super init]) == nil) {
        // This shouldn't happen, but we have to fufill our API contract
        populate_nserror(outError, PLCrashReporterErrorUnknown, @"Could not initialize superclass");
        return nil;
    }
    PLCF_DEBUG("YHCR: Created super class")


    /* Allocate the struct and attempt to parse */
    _decoder = malloc(sizeof(_PLCrashReportDecoder));
    
    PLCF_DEBUG("YHCR: Allocated decoder")
    _decoder->crashReport = [self decodeCrashData: encodedData error: outError];
    
    PLCF_DEBUG("YHCR: decoded crash report")
    /* Check if decoding failed. If so, outError has already been populated. */
    if (_decoder->crashReport == NULL) {
        goto error;
    }

    /* Report info (optional) */
    _uuid = NULL;
    if (_decoder->crashReport->report_info != NULL) {
        /* Report UUID (optional)
         * If our minimum supported target is bumped to (10.8+, iOS 6.0+), NSUUID should
         * be used instead. */
        if (_decoder->crashReport->report_info->has_uuid) {
            /* Validate the UUID length */
            if (_decoder->crashReport->report_info->uuid.len != sizeof(uuid_t)) {
                populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid , @"Report UUID value is not a standard 16 bytes");
                goto error;
            }

            CFUUIDBytes uuid_bytes;
            memcpy(&uuid_bytes, _decoder->crashReport->report_info->uuid.data, _decoder->crashReport->report_info->uuid.len);
            _uuid = CFUUIDCreateFromUUIDBytes(NULL, uuid_bytes);
        }
    }

    /* Machine info */
    if (_decoder->crashReport->machine_info != NULL) {
        _machineInfo = [self extractMachineInfo: _decoder->crashReport->machine_info error: outError];
        if (!_machineInfo)
            goto error;
    }

    /* System info */
    _systemInfo = [self extractSystemInfo: _decoder->crashReport->system_info processorInfo: _machineInfo.processorInfo error: outError];
    if (!_systemInfo)
        goto error;

    /* Application info */
    _applicationInfo = [self extractApplicationInfo: _decoder->crashReport->application_info error: outError];
    if (!_applicationInfo)
        goto error;
    
    /* Process info. Handle missing info gracefully -- it is only included in v1.1+ crash reports. */
    if (_decoder->crashReport->process_info != NULL) {
        _processInfo = [self extractProcessInfo: _decoder->crashReport->process_info error:outError];
        if (!_processInfo)
            goto error;
    }

    /* Signal info */
    _signalInfo = [self extractSignalInfo: _decoder->crashReport->signal error: outError];
    if (!_signalInfo)
        goto error;

    /* Mach exception info */
    if (_decoder->crashReport->signal != NULL && _decoder->crashReport->signal->mach_exception != NULL) {
        _machExceptionInfo = [self extractMachExceptionInfo: _decoder->crashReport->signal->mach_exception error: outError];
        if (!_machExceptionInfo)
            goto error;
    }

    /* Thread info */
    _threads = [self extractThreadInfo: _decoder->crashReport error: outError];
    if (!_threads)
        goto error;

    /* Image info */
    _images = [self extractImageInfo: _decoder->crashReport error: outError];
    if (!_images)
        goto error;

    /* Exception info, if it is available */
    if (_decoder->crashReport->exception != NULL) {
        _exceptionInfo = [self extractExceptionInfo: _decoder->crashReport->exception error: outError];
        if (!_exceptionInfo)
            goto error;
    }

    /* Custom data, if it is available */
    if (_decoder->crashReport->has_custom_data) {
        _customData = [NSData dataWithBytes:_decoder->crashReport->custom_data.data length:_decoder->crashReport->custom_data.len];
        if (!_customData)
            goto error;
    }

    return self;

error:
    return nil;
}

- (void) dealloc {
    if (_uuid != NULL)
        CFRelease(_uuid);

    /* Free the decoder state */
    if (_decoder != NULL) {
        if (_decoder->crashReport != NULL) {
            protobuf_c_message_free_unpacked((ProtobufCMessage *) _decoder->crashReport, NULL);
        }

        free(_decoder);
        _decoder = NULL;
    }
}

/**
 * Return the binary image containing the given address, or nil if no binary image
 * is found.
 *
 * @param address The address to search for.
 */
- (PLCrashReportBinaryImageInfo *) imageForAddress: (uint64_t) address {
    for (PLCrashReportBinaryImageInfo *imageInfo in self.images) {
        uint64_t normalizedBaseAddress = imageInfo.imageBaseAddress;
        if (normalizedBaseAddress <= address && address < (normalizedBaseAddress + imageInfo.imageSize))
            return imageInfo;
    }

    /* Not found */
    return nil;
}

// property getter. Returns YES if machine information is available.
- (BOOL) hasMachineInfo {
    if (_machineInfo != nil)
        return YES;
    return NO;
}

// property getter. Returns YES if process information is available.
- (BOOL) hasProcessInfo {
    if (_processInfo != nil)
        return YES;
    return NO;
}

// property getter. Returns YES if exception information is available.
- (BOOL) hasExceptionInfo {
    if (_exceptionInfo != nil)
        return YES;
    return NO;
}

@synthesize systemInfo = _systemInfo;
@synthesize machineInfo = _machineInfo;
@synthesize applicationInfo = _applicationInfo;
@synthesize processInfo = _processInfo;
@synthesize signalInfo = _signalInfo;
@synthesize machExceptionInfo = _machExceptionInfo;
@synthesize threads = _threads;
@synthesize images = _images;
@synthesize exceptionInfo = _exceptionInfo;
@synthesize uuidRef = _uuid;

@end


/**
 * @internal
 * Private Methods
 */
@implementation PLCrashReport (PrivateMethods)

/**
 * Decode the crash log message.
 *
 * @warning MEMORY WARNING. The caller is responsible for deallocating th ePlcrash__CrashReport instance
 * returned by this method via protobuf_c_message_free_unpacked().
 */
- (Plcrash__CrashReport *) decodeCrashData: (NSData *) data error: (NSError **) outError {
    const struct PLCrashReportFileHeader *header;
    const void *bytes;

    bytes = [data bytes];
    header = bytes;

    /* Verify that the crash log is sufficently large */
    if (sizeof(struct PLCrashReportFileHeader) >= [data length]) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, NSLocalizedString(@"Could not decode truncated crash log",
                                                                                             @"Crash log decoding error message"));
        return NULL;
    }

    /* Check the file magic */
    if (memcmp(header->magic, PLCRASH_REPORT_FILE_MAGIC, strlen(PLCRASH_REPORT_FILE_MAGIC)) != 0) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,NSLocalizedString(@"Could not decode invalid crash log header",
                                                                                            @"Crash log decoding error message"));
        return NULL;
    }

    /* Check the version */
    if(header->version != PLCRASH_REPORT_FILE_VERSION) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, [NSString stringWithFormat: NSLocalizedString(@"Could not decode unsupported crash report version: %d", 
                                                                                                                         @"Crash log decoding message"), header->version]);
        return NULL;
    }

    Plcrash__CrashReport *crashReport = plcrash__crash_report__unpack(NULL, [data length] - sizeof(struct PLCrashReportFileHeader), header->data);
    if (crashReport == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, NSLocalizedString(@"An unknown error occured decoding the crash report", 
                                                                                             @"Crash log decoding error message"));
        return NULL;
    }

    return crashReport;
}


/**
 * Extract system information from the crash log. Returns nil on error.
 *
 * @param systemInfo The system info from the protobuf file.
 * @param processorInfo The system info from the machine info. This may be nil for v1 reports, in which case the
 * information will be synthesized from the architecture in the @a systemInfo.
 * @param outError A pointer to an NSError object variable. If an error occurs, this pointer will contain an error
 * object indicating why the system info could not be extracted. If no error occurs, this parameter will be left
 * unmodified. You may specify nil for this parameter, and no error information will be provided.
 *
 * @return Returns the system information, or nil on failure.
 */
- (PLCrashReportSystemInfo *) extractSystemInfo: (Plcrash__CrashReport__SystemInfo *) systemInfo
                                  processorInfo: (PLCrashReportProcessorInfo *) processorInfo
                                          error: (NSError **) outError
{
    NSDate *timestamp = nil;
    NSString *osBuild = nil;
    
    /* Validate */
    if (systemInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing System Information section", 
                                           @"Missing sysinfo in crash report"));
        return nil;
    }
    
    if (systemInfo->os_version == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing System Information OS version field", 
                                           @"Missing sysinfo operating system in crash report"));
        return nil;
    }

    /* Set up the build, if available */
    if (systemInfo->os_build != NULL)
        osBuild = [NSString stringWithUTF8String: systemInfo->os_build];
    
    /* Set up the timestamp, if available */
    if (systemInfo->timestamp != 0)
        timestamp = [NSDate dateWithTimeIntervalSince1970: systemInfo->timestamp];

	/* v1 crash logs will not have machine info, so the only data available to
	 * us is the deprecated architecture field. From that we will generate a
	 * PLCrashReportProcessorInfo object so that library users don't have to
	 * get at the architecture information multiple ways. */
	if (processorInfo == nil) {
        processorInfo = [self synthesizeProcessorInfoFromArchitecture: systemInfo->architecture error: outError];
        if (processorInfo == nil)
            return nil;
    }
    
    /* Done */
    return [[PLCrashReportSystemInfo alloc] initWithOperatingSystem: (PLCrashReportOperatingSystem) systemInfo->operating_system
                                              operatingSystemVersion: [NSString stringWithUTF8String: systemInfo->os_version]
                                                operatingSystemBuild: osBuild
                                                        architecture: (PLCrashReportArchitecture) systemInfo->architecture
                                                       processorInfo: processorInfo
                                                           timestamp: timestamp];
}

/**
 * Extract processor information from the crash log. Returns nil on error.
 */
- (PLCrashReportProcessorInfo *) extractProcessorInfo: (Plcrash__CrashReport__Processor *) processorInfo error: (NSError **) outError {   
    /* Validate */
    if (processorInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing processor info section", 
                                           @"Missing processor info in crash report"));
        return nil;
    }

    return [[PLCrashReportProcessorInfo alloc] initWithTypeEncoding: (PLCrashReportProcessorTypeEncoding) processorInfo->encoding
                                                                type: processorInfo->type
                                                             subtype: processorInfo->subtype];
}

/**
 * Synthesize a processor information object from an architecture type. Returns nil on error.
 */
- (PLCrashReportProcessorInfo *) synthesizeProcessorInfoFromArchitecture: (Plcrash__Architecture) architecture error:(NSError **)outError {
	uint64_t processorType;
	uint64_t processorSubtype;
	switch (architecture) {
		case PLCRASH__ARCHITECTURE__X86_32:
			processorType = CPU_TYPE_X86;
			processorSubtype = CPU_SUBTYPE_X86_ALL;
			break;

		case PLCRASH__ARCHITECTURE__X86_64:
			processorType = CPU_TYPE_X86_64;
			processorSubtype = CPU_SUBTYPE_X86_64_ALL;
			break;

		case PLCRASH__ARCHITECTURE__PPC:
			processorType = CPU_TYPE_POWERPC;
			processorSubtype = CPU_SUBTYPE_POWERPC_ALL;
			break;

		case PLCRASH__ARCHITECTURE__PPC64:
			processorType = CPU_TYPE_POWERPC64;
			processorSubtype = CPU_SUBTYPE_POWERPC_ALL;
			break;

		case PLCRASH__ARCHITECTURE__ARMV6:
			processorType = CPU_TYPE_ARM;
			processorSubtype = CPU_SUBTYPE_ARM_V6;
			break;

		case PLCRASH__ARCHITECTURE__ARMV7:
			processorType = CPU_TYPE_ARM;
			processorSubtype = CPU_SUBTYPE_ARM_V7;
			break;

		default:
            populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,
                             NSLocalizedString(@"Crash report has an unknown architecture",
                                               @"Unknown architecture in crash report"));
			return nil;
	}

    return [[PLCrashReportProcessorInfo alloc] initWithTypeEncoding: PLCrashReportProcessorTypeEncodingMach
                                                                type: processorType
                                                             subtype: processorSubtype];
}

/**
 * Extract machine information from the crash log. Returns nil on error.
 */
- (PLCrashReportMachineInfo *) extractMachineInfo: (Plcrash__CrashReport__MachineInfo *) machineInfo error: (NSError **) outError {
    NSString *model = nil;
    PLCrashReportProcessorInfo *processorInfo = nil;

    /* Validate */
    if (machineInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing Machine Information section", 
                                           @"Missing machine_info in crash report"));
        return nil;
    }

    /* Set up the model, if available */
    if (machineInfo->model != NULL)
        model = [NSString stringWithUTF8String: machineInfo->model];

    /* Set up the processor info. */
    if (machineInfo->processor != NULL) {
        processorInfo = [self extractProcessorInfo: machineInfo->processor error: outError];
        if (processorInfo == nil)
            return nil;
    }

    /* Done */
    return [[PLCrashReportMachineInfo alloc] initWithModelName: model
                                                  processorInfo: processorInfo
                                                 processorCount: machineInfo->processor_count
                                          logicalProcessorCount: machineInfo->logical_processor_count];
}

/**
 * Extract application information from the crash log. Returns nil on error.
 */
- (PLCrashReportApplicationInfo *) extractApplicationInfo: (Plcrash__CrashReport__ApplicationInfo *) applicationInfo 
                                                    error: (NSError **) outError
{    
    NSString *marketingVersion = nil;
    
    /* Validate */
    if (applicationInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing Application Information section", 
                                           @"Missing app info in crash report"));
        return nil;
    }

    /* Identifier available? */
    if (applicationInfo->identifier == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing Application Information app identifier field", 
                                           @"Missing app identifier in crash report"));
        return nil;
    }

    /* Version available? */
    if (applicationInfo->version == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing Application Information app version field", 
                                           @"Missing app version in crash report"));
        return nil;
    }
    
    /* Marketing Version available? */
    if (applicationInfo->marketing_version != NULL) {
        marketingVersion = [NSString stringWithUTF8String: applicationInfo->marketing_version];
    }

    /* Done */
    NSString *identifier = [NSString stringWithUTF8String: applicationInfo->identifier];
    NSString *version = [NSString stringWithUTF8String: applicationInfo->version];

    return [[PLCrashReportApplicationInfo alloc] initWithApplicationIdentifier: identifier
                                                             applicationVersion: version
                                                    applicationMarketingVersion:marketingVersion];
}


/**
 * Extract process information from the crash log. Returns nil on error.
 */
- (PLCrashReportProcessInfo *) extractProcessInfo: (Plcrash__CrashReport__ProcessInfo *) processInfo 
                                            error: (NSError **) outError
{    
    /* Validate */
    if (processInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing Process Information section", 
                                           @"Missing process info in crash report"));
        return nil;
    }
    
    /* Name available? */
    NSString *processName = nil;
    if (processInfo->process_name != NULL)
        processName = [NSString stringWithUTF8String: processInfo->process_name];
    
    /* Path available? */
    NSString *processPath = nil;
    if (processInfo->process_path != NULL)
        processPath = [NSString stringWithUTF8String: processInfo->process_path];

    /* Start time available? */
    NSDate *startTime = nil;
    if (processInfo->has_start_time)
        startTime = [NSDate dateWithTimeIntervalSince1970: processInfo->start_time];
    
    /* Parent Name available? */
    NSString *parentProcessName = nil;
    if (processInfo->parent_process_name != NULL)
        parentProcessName = [NSString stringWithUTF8String: processInfo->parent_process_name];

    /* Required elements */
    NSUInteger processID = processInfo->process_id;
    NSUInteger parentProcessID = processInfo->parent_process_id;

    /* Done */
    return [[PLCrashReportProcessInfo alloc] initWithProcessName: processName
                                                        processID: processID
                                                      processPath: processPath
                                                 processStartTime: startTime
                                                parentProcessName: parentProcessName
                                                  parentProcessID: parentProcessID
                                                           native: processInfo->native];
}

/**
 * Extract symbol information from the crash log. Returns nil on error, or a PLCrashReportSymbolInfo
 * instance on success.
 */
- (PLCrashReportSymbolInfo *) extractSymbolInfo: (Plcrash__CrashReport__Symbol *) symbol error: (NSError **) outError {
    if (symbol == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,
                         NSLocalizedString(@"Crash report is missing symbol information",
                                           @"Missing symbol info in crash report"));
        return nil;
    }
    
    NSString *name = [NSString stringWithUTF8String: symbol->name];
    return [[PLCrashReportSymbolInfo alloc] initWithSymbolName: name
                                                   startAddress: symbol->start_address
                                                     endAddress: symbol->has_end_address ? symbol->end_address : 0];
}

/**
 * Extract stack frame information from the crash log. Returns nil on error, or a PLCrashReportStackFrameInfo
 * instance on success.
 */
- (PLCrashReportStackFrameInfo *) extractStackFrameInfo: (Plcrash__CrashReport__Thread__StackFrame *) stackFrame error: (NSError **) outError {
    /* There should be at least one thread */
    if (stackFrame == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,
                         NSLocalizedString(@"Crash report is missing stack frame information",
                                           @"Missing stack frame info in crash report"));
        return nil;
    }
    
    PLCrashReportSymbolInfo *symbolInfo = nil;
    if (stackFrame->symbol != NULL) {
        if ((symbolInfo = [self extractSymbolInfo: stackFrame->symbol error: outError]) == NULL)
            return nil;
    }
    uint64_t instructionPointer = stackFrame->pc;
    /*
     * Workaround to handle incorrectly collected reports by old PLCrashReporter versions.
     * This guard does nothing on correctly collected reports.
     */
    if (_machineInfo &&
        _machineInfo.processorInfo.type == CPU_TYPE_ARM64 &&
        _machineInfo.processorInfo.subtype == CPU_SUBTYPE_ARM64E) {
        instructionPointer &= ARM64_PTR_MASK;
    }
    return [[PLCrashReportStackFrameInfo alloc] initWithInstructionPointer: instructionPointer
                                                                symbolInfo: symbolInfo];
}

/**
 * Extract thread information from the crash log. Returns nil on error, or an array of PLCrashLogThreadInfo
 * instances on success.
 */
- (NSArray *) extractThreadInfo: (Plcrash__CrashReport *) crashReport error: (NSError **) outError {
    /* There should be at least one thread */
    if (crashReport->n_threads == 0) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,
                         NSLocalizedString(@"Crash report is missing thread state information",
                                           @"Missing thread info in crash report"));
        return nil;
    }

    /* Handle all threads */
    NSMutableArray *threadResult = [NSMutableArray arrayWithCapacity: crashReport->n_threads];
    for (size_t thr_idx = 0; thr_idx < crashReport->n_threads; thr_idx++) {
        Plcrash__CrashReport__Thread *thread = crashReport->threads[thr_idx];
        
        /* Fetch stack frames for this thread */
        NSMutableArray *frames = [NSMutableArray arrayWithCapacity: thread->n_frames];
        for (size_t frame_idx = 0; frame_idx < thread->n_frames; frame_idx++) {
            Plcrash__CrashReport__Thread__StackFrame *frame = thread->frames[frame_idx];
            PLCrashReportStackFrameInfo *frameInfo = [self extractStackFrameInfo: frame error: outError];
            if (frameInfo == nil)
                return nil;

            [frames addObject: frameInfo];
        }

        /* Fetch registers for this thread */
        NSMutableArray *registers = [NSMutableArray arrayWithCapacity: thread->n_registers];
        for (size_t reg_idx = 0; reg_idx < thread->n_registers; reg_idx++) {
            Plcrash__CrashReport__Thread__RegisterValue *reg = thread->registers[reg_idx];
            PLCrashReportRegisterInfo *regInfo;

            /* Handle missing register name (should not occur!) */
            if (reg->name == NULL) {
                populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, @"Missing register name in register value");
                return nil;
            }

            NSString *regiserType = nil;
            if (reg->type != NULL) {
                regiserType = [NSString stringWithUTF8String:reg->type];
            }

            NSString *registerContent = nil;
            if (reg->content != NULL) {
                registerContent = [NSString stringWithUTF8String:reg->content];
            }

            regInfo = [[PLCrashReportRegisterInfo alloc] initWithRegisterName: [NSString stringWithUTF8String: reg->name]
                                                              registerValue: reg->value
                                                                  registerType:regiserType
                                                                 registerValue:registerContent];
            [registers addObject: regInfo];
        }

        /* Create the thread info instance */
        PLCrashReportThreadInfo *threadInfo = [[PLCrashReportThreadInfo alloc] initWithThreadNumber: thread->thread_number
                                                                                   stackFrames: frames 
                                                                                       crashed: thread->crashed 
                                                                                     registers: registers];
        [threadResult addObject: threadInfo];
    }
    
    return threadResult;
}


/**
 * Extract binary image information from the crash log. Returns nil on error.
 */
- (NSArray *) extractImageInfo: (Plcrash__CrashReport *) crashReport error: (NSError **) outError {
    /* There should be at least one image */
    if (crashReport->n_binary_images == 0) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,
                         NSLocalizedString(@"Crash report is missing binary image information",
                                           @"Missing image info in crash report"));
        return nil;
    }

    /* Handle all records */
    NSMutableArray *images = [NSMutableArray arrayWithCapacity: crashReport->n_binary_images];
    for (size_t i = 0; i < crashReport->n_binary_images; i++) {
        Plcrash__CrashReport__BinaryImage *image = crashReport->binary_images[i];
        PLCrashReportBinaryImageInfo *imageInfo;

        /* Validate */
        if (image->name == NULL) {
            populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, @"Missing image name in image record");
            return nil;
        }

        /* Extract UUID value */
        NSData *uuid = nil;
        if (image->uuid.len == 0) {
            /* No UUID */
            uuid = nil;
        } else {
            uuid = [NSData dataWithBytes: image->uuid.data length: image->uuid.len];
        }
        assert(image->uuid.len == 0 || uuid != nil);
        
        /* Extract code type (if available). */
        PLCrashReportProcessorInfo *codeType = nil;
        if (image->code_type != NULL) {
            if ((codeType = [self extractProcessorInfo: image->code_type error: outError]) == nil)
                return nil;
        }


        imageInfo = [[PLCrashReportBinaryImageInfo alloc] initWithCodeType: codeType
                                                                baseAddress: image->base_address
                                                                       size: image->size
                                                                       name: [NSString stringWithUTF8String: image->name]
                                                                       uuid: uuid];
        [images addObject: imageInfo];
    }

    return images;
}

/**
 * Extract  exception information from the crash log. Returns nil on error.
 */
- (PLCrashReportExceptionInfo *) extractExceptionInfo: (Plcrash__CrashReport__Exception *) exceptionInfo
                                               error: (NSError **) outError
{
    /* Validate */
    if (exceptionInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing Exception Information section", 
                                           @"Missing appinfo in crash report"));
        return nil;
    }
    
    /* Name available? */
    if (exceptionInfo->name == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing exception name field", 
                                           @"Missing appinfo operating system in crash report"));
        return nil;
    }
    
    /* Reason available? */
    if (exceptionInfo->reason == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing exception reason field", 
                                           @"Missing appinfo operating system in crash report"));
        return nil;
    }
    
    /* Done */
    NSString *name = [NSString stringWithUTF8String: exceptionInfo->name];
    NSString *reason = [NSString stringWithUTF8String: exceptionInfo->reason];
    
    /* Fetch stack frames for this thread */
    NSMutableArray *frames = nil;
    if (exceptionInfo->n_frames > 0) {
        frames = [NSMutableArray arrayWithCapacity: exceptionInfo->n_frames];
        for (size_t frame_idx = 0; frame_idx < exceptionInfo->n_frames; frame_idx++) {
            Plcrash__CrashReport__Thread__StackFrame *frame = exceptionInfo->frames[frame_idx];
            PLCrashReportStackFrameInfo *frameInfo = [self extractStackFrameInfo: frame error: outError];
            if (frameInfo == nil)
                return nil;
            
            [frames addObject: frameInfo];
        }
    }

    if (frames == nil) {
        return [[PLCrashReportExceptionInfo alloc] initWithExceptionName: name reason: reason];
    } else {
        return [[PLCrashReportExceptionInfo alloc] initWithExceptionName: name
                                                                   reason: reason 
                                                              stackFrames: frames];
    }
}

/**
 * Extract signal information from the crash log. Returns nil on error.
 */
- (PLCrashReportSignalInfo *) extractSignalInfo: (Plcrash__CrashReport__Signal *) signalInfo
                                       error: (NSError **) outError
{
    /* Validate */
    if (signalInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing Signal Information section", 
                                           @"Missing appinfo in crash report"));
        return nil;
    }
    
    /* Name available? */
    if (signalInfo->name == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing signal name field", 
                                           @"Missing appinfo operating system in crash report"));
        return nil;
    }
    
    /* Code available? */
    if (signalInfo->code == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid, 
                         NSLocalizedString(@"Crash report is missing signal code field", 
                                           @"Missing appinfo operating system in crash report"));
        return nil;
    }
    
    /* Done */
    NSString *name = [NSString stringWithUTF8String: signalInfo->name];
    NSString *code = [NSString stringWithUTF8String: signalInfo->code];
    
    return [[PLCrashReportSignalInfo alloc] initWithSignalName: name code: code address: signalInfo->address];
}

/**
 * Extract Mach exception information from the crash log. Returns nil on error.
 */
- (PLCrashReportMachExceptionInfo *) extractMachExceptionInfo: (Plcrash__CrashReport__Signal__MachException *) machExceptionInfo
                                                        error: (NSError **) outError
{
    /* Validate */
    if (machExceptionInfo == NULL) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,
                         NSLocalizedString(@"Crash report is missing Mach Exception Information section",
                                           @"Missing mach exception info in crash report"));
        return nil;
    }
    
    /* Sanity check; there should really only ever be 2 */
    if (machExceptionInfo->n_codes > UINT8_MAX) {
        populate_nserror(outError, PLCrashReporterErrorCrashReportInvalid,
                         NSLocalizedString(@"Crash report includes too many Mach Exception codes",
                                           @"Invalid mach exception info in crash report"));
        return nil;
    }
    
    /* Extract the codes */
    NSMutableArray *codes = [NSMutableArray arrayWithCapacity: machExceptionInfo->n_codes];
    for (size_t i = 0; i < machExceptionInfo->n_codes; i++) {
        [codes addObject: [NSNumber numberWithUnsignedLongLong: machExceptionInfo->codes[i]]];
    }
    
    /* Done */
    return [[PLCrashReportMachExceptionInfo alloc] initWithType: machExceptionInfo->type codes: codes];
}

@end

/**
 * @internal
 
 * Populate an NSError instance with the provided information.
 *
 * @param error Error instance to populate. If NULL, this method returns
 * and nothing is modified.
 * @param code The error code corresponding to this error.
 * @param description A localized error description.
 */
static void populate_nserror (NSError **error, PLCrashReporterError code, NSString *description) {
    NSDictionary *userInfo;
    
    if (error == NULL)
        return;
    
    /* Create the userInfo dictionary */
    userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                description, NSLocalizedDescriptionKey,
                nil
                ];
    
    *error = [NSError errorWithDomain: PLCrashReporterErrorDomain code: code userInfo: userInfo];
}
