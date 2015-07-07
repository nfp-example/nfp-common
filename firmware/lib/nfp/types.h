/*
 * Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * This is a library of functions used by application firmware to
 * utilize various ME features in a uniform manner
 * 
 */
#ifndef _NFP__TYPES_H_
#define _NFP__TYPES_H_

/** uint64_32_t
 */
typedef union {
    uint64_t uint64;
    uint32_t uint32[2];
    struct {
        uint32_t uint32_hi;
        uint32_t uint32_lo;
    };
} uint64_32_t;

/** Close guard
 */
#endif /*_NFP__TYPES_H_ */
