default_host_cc = $(host_toolchain)gcc
default_host_cxx = $(host_toolchain)g++
default_host_ar = $(host_toolchain)ar
default_host_ranlib = $(host_toolchain)ranlib
default_host_strip = $(host_toolchain)strip
default_host_libtool = $(host_toolchain)libtool
default_host_install_name_tool = $(host_toolchain)install_name_tool
default_host_otool = $(host_toolchain)otool
default_host_nm = $(host_toolchain)nm

define add_host_tool_func
$(host_os)_$1?=$$(default_host_$1)
$(host_arch)_$(host_os)_$1?=$$($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1?=$$($(host_os)_$1)
host_$1=$$($(host_arch)_$(host_os)_$1)
endef

define add_host_flags_func
$(host_arch)_$(host_os)_$1 += $($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1 += $($(host_os)_$(release_type)_$1)
host_$1 = $$($(host_arch)_$(host_os)_$1)
host_$(release_type)_$1 = $$($(host_arch)_$(host_os)_$(release_type)_$1)
endef

$(foreach tool,cc cxx ar ranlib strip nm libtool otool install_name_tool,$(eval $(call add_host_tool_func,$(tool))))
$(foreach flags,cflags cxxflags cppflags ldflags, $(eval $(call add_host_flags_func,$(flags))))
