deps_config := \
	/home/lw/Project/compiler/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
