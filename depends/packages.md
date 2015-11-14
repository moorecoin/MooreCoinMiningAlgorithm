each recipe consists of 3 main parts: defining identifiers, setting build
variables, and defining build commands.

the package "mylib" will be used here as an example

general tips:
- mylib_foo is written as $(package)_foo in order to make recipes more similar.

## identifiers
each package is required to define at least these variables:

    $(package)_version:
    version of the upstream library or program. if there is no version, a
    placeholder such as 1.0 can be used.

    $(package)_download_path:
    location of the upstream source, without the file-name. usually http or
    ftp.

    $(package)_file_name:
    the upstream source filename available at the download path.

    $(package)_sha256_hash:
    the sha256 hash of the upstream file

these variables are optional:

    $(package)_build_subdir:
    cd to this dir before running configure/build/stage commands.
    
    $(package)_download_file:
    the file-name of the upstream source if it differs from how it should be
    stored locally. this can be used to avoid storing file-names with strange
    characters.
    
    $(package)_dependencies:
    names of any other packages that this one depends on.
    
    $(package)_patches:
    filenames of any patches needed to build the package

    $(package)_extra_sources:
    any extra files that will be fetched via $(package)_fetch_cmds. these are
    specified so that they can be fetched and verified via 'make download'.


## build variables:
after defining the main identifiers, build variables may be added or customized
before running the build commands. they should be added to a function called
$(package)_set_vars. for example:

    define $(package)_set_vars
    ...
    endef

most variables can be prefixed with the host, architecture, or both, to make
the modifications specific to that case. for example:

    universal:     $(package)_cc=gcc
    linux only:    $(package)_linux_cc=gcc
    x86_64 only:       $(package)_x86_64_cc = gcc
    x86_64 linux only: $(package)_x86_64_linux_cc = gcc

these variables may be set to override or append their default values.

    $(package)_cc
    $(package)_cxx
    $(package)_objc
    $(package)_objcxx
    $(package)_ar
    $(package)_ranlib
    $(package)_libtool
    $(package)_nm
    $(package)_cflags
    $(package)_cxxflags
    $(package)_ldflags
    $(package)_cppflags
    $(package)_config_env
    $(package)_build_env
    $(package)_stage_env
    $(package)_build_opts
    $(package)_config_opts

the *_env variables are used to add environment variables to the respective
commands.

many variables respect a debug/release suffix as well, in order to use them for
only the appropriate build config. for example:

    $(package)_cflags_release = -o3
    $(package)_cflags_i686_debug = -g
    $(package)_config_opts_release = --disable-debug

these will be used in addition to the options that do not specify
debug/release. all builds are considered to be release unless debug=1 is set by
the user. other variables may be defined as needed.

## build commands:

  for each build, a unique build dir and staging dir are created. for example,
  `work/build/mylib/1.0-1adac830f6e` and `work/staging/mylib/1.0-1adac830f6e`.

  the following build commands are available for each recipe:

    $(package)_fetch_cmds:
    runs from: build dir
    fetch the source file. if undefined, it will be fetched and verified
    against its hash.

    $(package)_extract_cmds:
    runs from: build dir
    verify the source file against its hash and extract it. if undefined, the
    source is assumed to be a tarball.

    $(package)_preprocess_cmds:
    runs from: build dir/$(package)_build_subdir
    preprocess the source as necessary. if undefined, does nothing.

    $(package)_config_cmds:
    runs from: build dir/$(package)_build_subdir
    configure the source. if undefined, does nothing.

    $(package)_build_cmds:
    runs from: build dir/$(package)_build_subdir
    build the source. if undefined, does nothing.

    $(package)_stage_cmds:
    runs from: build dir/$(package)_build_subdir
    stage the build results. if undefined, does nothing.

  the following variables are available for each recipe:
    
    $(1)_staging_dir: package's destination sysroot path
    $(1)_staging_prefix_dir: prefix path inside of the package's staging dir
    $(1)_extract_dir: path to the package's extracted sources
    $(1)_build_dir: path where configure/build/stage commands will be run
    $(1)_patch_dir: path where the package's patches (if any) are found

notes on build commands:

for packages built with autotools, $($(package)_autoconf) can be used in the
configure step to (usually) correctly configure automatically. any
$($(package)_config_opts) will be appended.

most autotools projects can be properly staged using:

    $(make) destdir=$($(package)_staging_dir) install
