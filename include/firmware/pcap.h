/** Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * @file          include/firmware/pktgen.h
 * @brief         Packet generator firmware include shared with host
 *
 */

/** Includes
 */
#include <stdint.h> 

/** Defines
 */
#ifndef STRINGIFY
#define __STRINGIFY(x) #x
#define STRINGIFY(x) __STRINGIFY(x)
#endif

#define PCAP_HOST_CLS_SHARED_DATA_SIZE 64
#define PCAP_HOST_CLS_RING_SIZE 1024
#define PCAP_HOST_CLS_RING_SIZE__STR STRINGIFY(PKTGEN_CLS_RING_SIZE)

/** struct pcap_cls_host
 */
struct pcap_cls_host {
    uint32_t wptr;
};

