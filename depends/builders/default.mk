default_build_cc = gcc
default_build_cxx = g++
default_build_ar = ar
default_build_ranlib = ranlib
default_build_strip = strip
default_build_nm = nm
default_build_otool = otool
default_build_install_name_tool = install_name_tool

define add_build_tool_func
build_$(build_os)_$1 ?= $$(default_build_$1)
build_$(build_arch)_$(build_os)_$1 ?= $$(build_$(build_os)_$1)
build_$1=$$(build_$(build_arch)_$(build_os)_$1)
endef
$(foreach var,cc cxx ar ranlib nm strip sha256sum download otool install_name_tool,$(eval $(call add_build_tool_func,$(var))))
define add_build_flags_func
build_$(build_arch)_$(build_os)_$1 += $(build_$(build_os)_$1)
build_$1=$$(build_$(build_arch)_$(build_os)_$1)
endef
$(foreach flags, cflags cxxflags ldflags, $(eval $(call add_build_flags_func,$(flags))))
