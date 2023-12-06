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

-include $(base_path)/include/config/auto.conf
-include $(base_path)/include/config/auto.conf.cmd

CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -Iinc -Iinclude/generated

LDFLAGS += -lyaml-cpp

ifeq '$(CONFIG_OPEN_DEBUG)' 'y'
CFLAGS += -g -Og
else
CFLAGS += -O2
endif

SRCS = main.cc
SRCS += src/tok.cc src/scan.cc src/out.cc src/parse/parse_base.cc src/util.cc

ifdef CONFIG_SELECT_CODE_GEN_FORMAT_DUMMY
SRCS += src/gen_dummy.cc
endif
ifdef CONFIG_SELECT_CODE_GEN_FORMAT_LLVM
SRCS += src/gen_llvm.cc
endif

ifdef CONFIG_SELECT_PARSER_TOP_DOWN
SRCS += src/parse/parse_top_down.cc
endif
ifdef CONFIG_SELECT_PARSER_LL1_SHEET
SRCS += src/parse/ll1_sheet/base.cc
SRCS += src/parse/ll1_sheet/parse_lex.cc
endif
ifdef CONFIG_SELECT_PARSER_LR1
SRCS += src/parse/lr1/base.cc
SRCS += src/parse/lr1/gramma.cc
SRCS += src/parse/lr1/reduce.cc
SRCS += src/parse/lr1/parse.cc
endif

ifdef CONFIG_SELECT_REDUCE_FN_SELFWRITE
REDUCE_SRCS = $(shell find $(CONFIG_REDUCE_FN_PATH) -type f -regex '.*\.\(\(c\)\|\(cpp\)\|\(cc\)\|\(cxx\)\)')
SRCS += $(SELF_REDUCE_SRCS)
endif
ifdef CONFIG_SELECT_REDUCE_FN_PREWRITE
REDUCE_SRCS = $(shell find $(CONFIG_REDUCE_FN_PATH) -type f -regex '.*\.\(\(c\)\|\(cpp\)\|\(cc\)\|\(cxx\)\)')
SRCS += $(REDUCE_SRCS)
endif

OBJS = $(SRCS:%.cc=$(build_path)/%.o)

$(build_path)/%.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -c $< -o $@

$(build_path)/neko_cc: $(OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) $^ -o $@
	
all: $(build_path)/neko_cc

clean:
	rm -rf $(build_path)

.PHONY: all