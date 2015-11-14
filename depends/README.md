### usage

to build dependencies for the current arch+os:

    make

to build for another arch/os:

    make host=host-platform-triplet

for example:

    make host=x86_64-w64-mingw32 -j4

a prefix will be generated that's suitable for plugging into bitcoin's
configure. in the above example, a dir named i686-w64-mingw32 will be
created. to use it for bitcoin:

    ./configure --prefix=`pwd`/depends/x86_64-w64-mingw32

common `host-platform-triplets` for cross compilation are:

- `i686-w64-mingw32` for win32
- `x86_64-w64-mingw32` for win64
- `x86_64-apple-darwin11` for macosx
- `arm-linux-gnueabihf` for linux arm

no other options are needed, the paths are automatically configured.

dependency options:
the following can be set when running make: make foo=bar

    sources_path: downloaded sources will be placed here
    base_cache: built packages will be placed here
    sdk_path: path where sdk's can be found (used by osx)
    fallback_download_path: if a source file can't be fetched, try here before giving up
    no_qt: don't download/build/cache qt and its dependencies
    no_wallet: don't download/build/cache libs needed to enable the wallet
    no_upnp: don't download/build/cache packages needed for enabling upnp
    debug: disable some optimizations and enable more runtime checking

if some packages are not built, for example `make no_wallet=1`, the appropriate
options will be passed to bitcoin's configure. in this case, `--disable-wallet`.

additional targets:

    download: run 'make download' to fetch all sources without building them
    download-osx: run 'make download-osx' to fetch all sources needed for osx builds
    download-win: run 'make download-win' to fetch all sources needed for win builds
    download-linux: run 'make download-linux' to fetch all sources needed for linux builds

### other documentation

- [description.md](description.md): general description of the depends system
- [packages.md](packages.md): steps for adding packages

