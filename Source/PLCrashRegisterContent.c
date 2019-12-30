//
//  PLCrashRegisterContent.c
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 11/14/19.
//

#include "PLCrashRegisterContent.h"
#include "PLObjC.h"
#include "PLMemory.h"
#include "PLString.h"

static inline bool plregister_is_valid_pointer(const uintptr_t address)
{
    if(address == (uintptr_t)NULL)
    {
        return false;
    }

    if(plobjc_is_tagged_pointer((const void*)address))
    {
        if(!plobjc_is_valid_tagged_pointer((const void*)address))
        {
            return false;
        }
    }

    return true;
}

bool plregister_is_notable_address(const uintptr_t address)
{
    if(!plregister_is_valid_pointer(address))
    {
        return false;
    }
    
    const void* object = (const void*)address;

    if(plstring_is_valid(object))
    {
        return true;
    }

    return false;
}
