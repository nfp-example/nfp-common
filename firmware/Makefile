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
# @file        firmware/Makefile
# @brief       Makefile for the firmware
#

FW_BUILD = $(FIRMWARE_DIR)/build
FW_NFFW  = $(FIRMWARE_DIR)/nffw

NFAS = $(NETRONOME)/bin/nfas
NFAS_BASE_INC = 
NFAS_BASE_DEFS = 
# -indirect_ref_format_nfp6000  BREAKS codeless builds
CHIP = nfp-6xxx-a0
CHIP = NFP-624A-0C-A0
CHIP = NFP-624A-0C-B0
NFAS_BASE_FLAGS = -chip $(CHIP)

NFCC = $(NETRONOME)/bin/nfcc
NFCC_BASE_INC = -I$(FIRMWARE_DIR)/lib -I$(INCLUDE_DIR) -I$(NETRONOME)/components/standardlibrary/microc/include -I$(NETRONOME)/include
NFCC_BASE_DEFS = 
NFCC_BASE_FLAGS = -Zi -W3 -Qspill=7 -Qnctx_mode=8 -single_dram_signal
NFCC_BASE_CLIBS = 

NFLD = $(NETRONOME)/bin/nfld
NFLD_BASE_FLAGS = -chip $(CHIP) -g -elf64

include $(FIRMWARE_DIR)/Makefile.templates
include $(FIRMWARE_DIR)/Makefile.apps

ALL: firmware_all

clean: firmware_clean

firmware_all: all_nffw

all_nffw:

firmware_clean:
	rm -f $(FW_BUILD)/*.map
	rm -f $(FW_BUILD)/*.list
	rm -f $(FW_BUILD)/*.obj
	rm -f $(FW_NFFW)/*.nffw
