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
NFAS_BASE_FLAGS = -indirect_ref_format_nfp6000

NFCC = $(NETRONOME)/bin/nfcc
NFCC_BASE_INC = -I$(FIRMWARE_DIR)/lib
NFCC_BASE_DEFS = 
NFCC_BASE_FLAGS = 
NFCC_BASE_CLIBS = 

NFLD = $(NETRONOME)/bin/nfld
NFLD_BASE_FLAGS = 

include $(FIRMWARE_DIR)/Makefile.templates
include $(FIRMWARE_DIR)/Makefile.apps

firmware_all: all_nffw

all_nffw:

firmware_clean:
	rm -f $(FW_BUILD)/*.map
	rm -f $(FW_BUILD)/*.list
	rm -f $(FW_BUILD)/*.obj
	rm -f $(FW_NFFW)/*.nffw