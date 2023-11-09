base_path = $(shell pwd)
build_path = $(base_path)/build

kconfig_path = $(base_path)/Kconfig

conf = $(base_path)/configs/conf
mconf = $(base_path)/configs/mconf

defalut: help

menuconfig:
	$(mconf) $(kconfig_path)
	$(conf) --syncconfig $(kconfig_path)

savedefconfig:
	$(conf) --savedefconfig=configs/defconfig $(kconfig_path)

help:
	@echo  '  menuconfig	  - Update current config utilising a menu based program'
	@echo  '  savedefconfig   - Save current config as configs/defconfig (minimal config)'
	@echo  '  help		  - Show this help message'
	@echo  '  all		  - Build all targets'

.PHONY: menuconfig savedefconfig help

include $(base_path)/include/config/auto.conf
-include $(base_path)/include/config/auto.conf.cmd

CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -Iinc -Iinclude/generated

ifeq '$(CONFIG_OPEN_DEBUG)' 'y'
CFLAGS += -g -Og
else
CFLAGS += -O2
endif

SRCS = main.cc
SRCS += src/tok.cc src/scan.cc src/out.cc src/anal.cc

ifdef CONFIG_SELECT_CODE_GEN_FORMAT_DUMMY
SRCS += src/gen_dummy.cc
endif
ifdef CONFIG_SELECT_CODE_GEN_FORMAT_LLVM
SRCS += src/gen_llvm.cc
endif

OBJS = $(SRCS:%.cc=$(build_path)/%.o)

$(build_path)/%.o: %.cc
	@mkdir -p $(dir $@)
	@$(CXX) $(CFLAGS) -c $< -o $@

$(build_path)/neko_cc: $(OBJS)
	@$(CXX) $(CFLAGS) $^ -o $@
	
all: $(build_path)/neko_cc

.PHONY: all