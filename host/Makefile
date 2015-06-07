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
# @file        host/Makefile
# @brief       Makefile for the host applications
#
ALL: all_host

clean: clean_host

HOST_SRC_DIR   = $(HOST_DIR)/src
HOST_BUILD_DIR = $(HOST_DIR)/build
HOST_BIN_DIR   = $(HOST_DIR)/bin

#CC=gcc
INCLUDES=-I$(NETRONOME)/include
LIBS=-L$(NETRONOME)/lib -L/usr/lib/x86_64-linux-gnu/ -lnfp -lnfp_nffw -lhugetlbfs -ljansson

CC=gcc $(INCLUDES) -g -Wall -Werror
LD=gcc -g

include $(HOST_DIR)/Makefile.templates
include $(HOST_DIR)/Makefile.apps

clean_host:
	rm $(HOST_BUILD_DIR)/*.o

$(HOST_BUILD_DIR)/%.o: $(HOST_SRC_DIR)/%.c
	$(CC) -c -o $@ -I$(HOST_DIR)/src $<
