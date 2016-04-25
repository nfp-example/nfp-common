#a Copyright (C) 2015,  Gavin J Stark.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# @file        Makefile
# @brief       Top level makefile to set paths and include submakes
#

NFP_COMMON    := $(abspath $(CURDIR))
NETRONOME = /opt/netronome

PYTHON_DIR    = $(NFP_COMMON)/python
SCRIPTS_DIR   = $(NFP_COMMON)/scripts
FIRMWARE_DIR  = $(NFP_COMMON)/firmware
INCLUDE_DIR   = $(NFP_COMMON)/include
HOST_DIR      = $(NFP_COMMON)/host
DOCS_DIR      = $(NFP_COMMON)/docs

ALL:

clean:

test:

help:
	@echo "To make for a Starfighter, use:"
	@echo "  make CHIP=NFP-624A-0C-A0"
	@echo "or similar"
	@echo ""
	@echo "If the tools are not installed in /opt/netronome"
	@echo "  make NETRONOME=/path/to/tools"
	@echo ""
	@echo "To build some things without a real NFP library"
	@echo "  make DUMMY_NFP=y"
	@echo ""
	@echo "To find shared memory segments"
	@echo "  ipcs"
	@echo ""
	@echo "To remove a shared memory segment"
	@echo "  ipcrm -m <shmid>"
	@echo ""

include $(FIRMWARE_DIR)/Makefile
include $(HOST_DIR)/Makefile
include $(SCRIPTS_DIR)/Makefile
#include $(DOCS_DIR)/Makefile
