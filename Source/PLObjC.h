//
//  KSObjC.h
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


#ifndef HDR_PLObjC_h
#define HDR_PLObjC_h

#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stdint.h>


typedef enum
{
    PLObjCTypeUnknown = 0,
    PLObjCTypeClass,
    PLObjCTypeObject,
    PLObjCTypeBlock,
} PLObjCType;

typedef enum
{
    PLObjCClassTypeUnknown = 0,
    PLObjCClassTypeString,
    PLObjCClassTypeDate,
    PLObjCClassTypeURL,
    PLObjCClassTypeArray,
    PLObjCClassTypeDictionary,
    PLObjCClassTypeNumber,
    PLObjCClassTypeException,
} PLObjCClassType;

//======================================================================
#pragma mark - Basic Objective-C Queries -
//======================================================================

/** Check if a pointer is a tagged pointer or not.
 *
 * @param pointer The pointer to check.
 * @return true if it's a tagged pointer.
 */
bool plobjc_is_tagged_pointer(const void* const pointer);

/** Check if a pointer is a valid tagged pointer.
 *
 * @param pointer The pointer to check.
 * @return true if it's a valid tagged pointer.
 */
bool plobjc_is_valid_tagged_pointer(const void* const pointer);

#ifdef __cplusplus
}
#endif

#endif // HDR_PLObjC_h
