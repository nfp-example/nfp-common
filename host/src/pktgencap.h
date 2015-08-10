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
 * @file          pktgencap.h
 * @brief         Packet generator/capture memory support
 *
 *
 * This set of library functions supports the packet generator firmware
 * and its memory initialization. It provides functions for loading and
 * populating the NFP memories as required by the packet generator
 * firmware.
 * 
 * The memory layout consists of a schedule, a set of scripts, and
 * regions of packet data.
 *
 * The schedule contains sets of flow-packet entries, each entry of
 * which contains a transmit time, packet MU address (including
 * island), length, flags and a script offset.
 *
 * The file/data to be loaded consists of a schedule file, a script
 * file, and at least one packet data file.
 *
 * The loading process requires allocation of the NFP memory, and
 * fixing-up of packet addresses from the schedule to match the actual
 * memory addresss of packet data.
 * 
 */

/** Includes
 */
#include <stdint.h> 

/** PKTGEN_IPC_*
 */
enum {
    PKTGEN_IPC_SHUTDOWN,
    PKTGEN_IPC_HOST_CMD,
    PKTGEN_IPC_DUMP_BUFFERS
};

/** pktgen_ipc_msg
 */
struct pktgen_ipc_msg {
    int reason;
};
