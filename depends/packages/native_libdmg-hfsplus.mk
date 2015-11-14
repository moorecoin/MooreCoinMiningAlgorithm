package=native_libdmg-hfsplus
$(package)_version=0.1
$(package)_download_path=https://github.com/theuni/libdmg-hfsplus/archive
$(package)_file_name=libdmg-hfsplus-v$($(package)_version).tar.gz
$(package)_sha256_hash=6569a02eb31c2827080d7d59001869ea14484c281efab0ae7f2b86af5c3120b3
$(package)_build_subdir=build

define $(package)_preprocess_cmds
  mkdir build
endef

define $(package)_config_cmds
  cmake -dcmake_install_prefix:path=$(build_prefix)/bin ..
endef

define $(package)_build_cmds
  $(make) -c dmg
endef

define $(package)_stage_cmds
  $(make) destdir=$($(package)_staging_dir) -c dmg install
endef
