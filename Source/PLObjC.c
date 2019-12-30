//
//  KSObjC.c
//
//  Created by Karl Stenerud on 2012-08-30.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#include "PLObjC.h"

#include "PLMemory.h"

//#include "KSLogger.h"

#if __IPHONE_OS_VERSION_MAX_ALLOWED > 70000
#include <objc/NSObjCRuntime.h>
#else
#if __LP64__ || (TARGET_OS_EMBEDDED && !TARGET_OS_IPHONE) || TARGET_OS_WIN32 || NS_BUILD_32_LIKE_64
typedef long NSInteger;
typedef unsigned long NSUInteger;
#else
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBase.h>
#include <CoreGraphics/CGBase.h>
#include <inttypes.h>
#include <objc/runtime.h>
#include <objc/objc.h>


#define kPLMaxNameLength 128

//======================================================================
#pragma mark - Macros -
//======================================================================

// Compiler hints for "if" statements
#define likely_if(x) if(__builtin_expect(x,1))
#define unlikely_if(x) if(__builtin_expect(x,0))


//======================================================================
#pragma mark - Types -
//======================================================================

typedef enum
{
    ClassSubtypeNone = 0,
    ClassSubtypeCFArray,
    ClassSubtypeNSArrayMutable,
    ClassSubtypeNSArrayImmutable,
    ClassSubtypeCFString,
} ClassSubtype;

typedef struct
{
    const char* name;
    PLObjCClassType type;
} ClassData;


//======================================================================
#pragma mark - Globals -
//======================================================================

static ClassData g_tagged_class_data[] =
{
    {"NSAtom",               PLObjCClassTypeUnknown},
    {NULL,                   PLObjCClassTypeUnknown},
    {"NSString",             PLObjCClassTypeString},
    {"NSNumber",             PLObjCClassTypeNumber},
    {"NSIndexPath",          PLObjCClassTypeUnknown},
    {"NSManagedObjectID",    PLObjCClassTypeUnknown},
    {"NSDate",               PLObjCClassTypeDate},
    {NULL,                   PLObjCClassTypeUnknown},
};
static int g_tagged_class_data_count = sizeof(g_tagged_class_data) / sizeof(*g_tagged_class_data);


//======================================================================
#pragma mark - Utility -
//======================================================================

#if SUPPORT_TAGGED_POINTERS
static bool plobjc_is_tagged_pointer_internal(const void* pointer) {return (((uintptr_t)pointer) & TAG_MASK) != 0; }
static int  plobjc_get_tagged_slot(const void* pointer) { return (int)((((uintptr_t)pointer) >> TAG_SLOT_SHIFT) & TAG_SLOT_MASK); }
#else
static bool plobjc_is_tagged_pointer_internal(__unused const void* pointer) { return false; }
static int  plobjc_get_tagged_slot(__unused const void* pointer) { return 0; }
#endif

/** Get class data for a tagged pointer.
 *
 * @param object The tagged pointer.
 * @return The class data.
 */
static const ClassData* plobjc_get_class_data_from_tagged_pointer(const void* const object)
{
    int slot =  plobjc_get_tagged_slot(object);
    return &g_tagged_class_data[slot];
}

static bool plobjc_is_valid_tagged_pointer_internal(const void* object)
{
    if(plobjc_is_tagged_pointer_internal(object))
    {
        if( plobjc_get_tagged_slot(object) <= g_tagged_class_data_count)
        {
            const ClassData* classData = plobjc_get_class_data_from_tagged_pointer(object);
            return classData->type != PLObjCClassTypeUnknown;
        }
    }
    return false;
}

//======================================================================
#pragma mark - General Queries -
//======================================================================


bool plobjc_is_tagged_pointer(const void* const pointer)
{
    return plobjc_is_tagged_pointer_internal(pointer);
}

bool plobjc_is_valid_tagged_pointer(const void* const pointer)
{
    return plobjc_is_valid_tagged_pointer_internal(pointer);
}
