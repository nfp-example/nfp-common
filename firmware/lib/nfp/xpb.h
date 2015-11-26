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
 * @file          firmware/lib/nfp/xpb.c
 * @brief         Library for NFP XPB transaction functions
 */
#ifndef _NFP__XPB_H_
#define _NFP__XPB_H_

/** Includes required
 */
#include <stdint.h>

/** xpb_read - Read an XPB register given base and offset
 * 
 * @param base  Base address, usually global + island + device
 * @param ofs   Offset from base address, usually register address
 *
 */
__intrinsic uint32_t xpb_read(uint32_t base, int ofs);

/** xpb_write - Write to an XPB register given base and offset
 * 
 * @param base  Base address, usually global + island + device
 * @param ofs   Offset from base address, usually register address
 * @param data  Data to be written to the XPB register
 *
 */
__intrinsic void xpb_write(uint32_t base, int ofs, uint32_t data);

/** Close guard
 */
#endif /*_NFP__XPB_H_ */
