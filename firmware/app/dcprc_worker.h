/*a Copyright */
/**
 Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
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
 * @file        pcap_lib.h
 * @brief       A simple packet capture system
 *
 * This is a library to support a PCAP packet capture to a host x86 system
 * 
 */


/*a Open guard
 */
#ifndef _DCPRC_WORKER_H_
#define _DCPRC_WORKER_H_

/*a Includes */
#include "firmware/data_coproc.h"

/*f dcprc_worker_null */
/**
 * @brief Main loop for the 'null' data coprocessor worker
 */
void dcprc_worker_null(void);

/*f dcprc_worker_null_init */
/**
 * @brief Initialize a 'null' data coprocessor worker
 */
void dcprc_worker_null_init(void);

/*a Close guard
 */
#endif /*_DCPRC_WORKER_H_ */
