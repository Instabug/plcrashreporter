//
//  PLCrashRegisterContent.h
//  CrashReporter-MacOSX
//
//  Created by Yousef Hamza on 11/14/19.
//

#ifndef PLCrashRegisterContent_h
#define PLCrashRegisterContent_h

#include <stdio.h>
#include <objc/runtime.h>

/**
* Check if the address that the register has points to a notable memory address
*
* @param address Memory address in register to test
*
* @return Returns true if address is notable, false otherwise.
*/
bool plregister_is_notable_address(const uintptr_t address);

#endif /* PLCrashRegisterContent_h */
