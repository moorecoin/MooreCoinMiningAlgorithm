package=miniupnpc
$(package)_version=1.9.20140701
$(package)_download_path=http://miniupnp.free.fr/files
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=26f3985bad7768b8483b793448ae49414cdc4451d0ec83e7c1944367e15f9f07

define $(package)_set_vars
$(package)_build_opts=cc="$($(package)_cc)"
$(package)_build_opts_darwin=os=darwin
$(package)_build_opts_mingw32=-f makefile.mingw
$(package)_build_env+=cflags="$($(package)_cflags) $($(package)_cppflags)" ar="$($(package)_ar)"
endef

define $(package)_preprocess_cmds
  mkdir dll && \
  sed -e 's|miniupnpc_version_string \"version\"|miniupnpc_version_string \"$($(package)_version)\"|' -e 's|os/version|$(host)|' miniupnpcstrings.h.in > miniupnpcstrings.h && \
  sed -i.old "s|miniupnpcstrings.h: miniupnpcstrings.h.in wingenminiupnpcstrings|miniupnpcstrings.h: miniupnpcstrings.h.in|" makefile.mingw
endef

define $(package)_build_cmds
	$(make) libminiupnpc.a $($(package)_build_opts)
endef

define $(package)_stage_cmds
	mkdir -p $($(package)_staging_prefix_dir)/include/miniupnpc $($(package)_staging_prefix_dir)/lib &&\
	install *.h $($(package)_staging_prefix_dir)/include/miniupnpc &&\
	install libminiupnpc.a $($(package)_staging_prefix_dir)/lib
endef
