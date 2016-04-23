/*a Copyright */
/**
 * Copyright (C) 2015-2016,  Gavin J Stark.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file          firmware/lib/nfp/types.h
 * @brief         Types required throughout the NFP library
 *
 * This is a library of types used by the NFP library
 * 
 */
/*a Open guard */
#ifndef _NFP__TYPES_H_
#define _NFP__TYPES_H_

/*t uint64_32_t */
/**
 * Union of uint64 and two uint32s in a big-word-endian fashion
 * (i.e. the opposite of the NFP hardware, but this is what the NFCC
 * compiler supports) This permits operations on 64-bit quantities
 * (such as writing from a link symbol) and extraction as two 32-bit
 * values, without using shifting.
 *
 * The compiler is not good at optimizing shifting. 
 */
typedef union {
    /** Represent 64-bits as a 64-bit int (compiler treats this as big
     * word endian) **/
    uint64_t uint64;
    /** Represent as two 32-bit integers, unnamed, in case an array is
     * needed */
    uint32_t uint32[2];
    /** Represent as a structure of high/low */
    struct {
        /** High 32-bits, big-word-endian **/
        uint32_t uint32_hi;
        /** Low 32-bits, big-word-endian **/
        uint32_t uint32_lo;
    };
} uint64_32_t;

/*a Close guard
 */
#endif /*_NFP__TYPES_H_ */
