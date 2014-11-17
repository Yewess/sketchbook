/*
 * Example sketch for eep library
 * Copyright (C) 2014 Christopher C. Evich
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef EEP_TEST_H
#define EEP_TEST_H

#define EEPDEBUG
#include <eep.h>

// Arbitrary class of data to persist w/in EEProm.
// Must be "standard layout" class or have static initialized members
class Data {
    public:
    char h[7] = "hello";
    char w[7] = "world";
    uint32_t answer = 42;
};

const Data defaults;  // Used when contents are unset/invalid

// Increment version number whenever Data type format changes (above)
typedef Eep::Eep<Data, 5> Eep_type;  // Create template class alias

// Staticly allocate space in EEProm area w/ EEMEM macro
// defined in <avr/eeprom.h>
Eep_type::Block _data EEMEM;  // Allocate space in EEProm area
                              // NEVER reference anything other than address
                              // from code!

#endif  // EEP_TEST_H
