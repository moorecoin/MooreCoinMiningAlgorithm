this is a system of building and caching dependencies necessary for building bitcoin. 
there are several features that make it different from most similar systems:

### it is designed to be builder and host agnostic

in theory, binaries for any target os/architecture can be created, from a
builder running any os/architecture. in practice, build-side tools must be
specified when the defaults don't fit, and packages must be amended to work
on new hosts. for now, a build architecture of x86_64 is assumed, either on
linux or osx.

### no reliance on timestamps

file presence is used to determine what needs to be built. this makes the
results distributable and easily digestable by automated builders.

### each build only has its specified dependencies available at build-time.

for each build, the sysroot is wiped and the (recursive) dependencies are
installed. this makes each build deterministic, since there will never be any
unknown files available to cause side-effects.

### each package is cached and only rebuilt as needed.

before building, a unique build-id is generated for each package. this id
consists of a hash of all files used to build the package (makefiles, packages,
etc), and as well as a hash of the same data for each recursive dependency. if
any portion of a package's build recipe changes, it will be rebuilt as well as
any other package that depends on it. if any of the main makefiles (makefile, 
funcs.mk, etc) are changed, all packages will be rebuilt. after building, the
results are cached into a tarball that can be re-used and distributed.

### package build results are (relatively) deterministic.

each package is configured and patched so that it will yield the same
build-results with each consequent build, within a reasonable set of
constraints. some things like timestamp insertion are unavoidable, and are
beyond the scope of this system. additionally, the toolchain itself must be
capable of deterministic results. when revisions are properly bumped, a cached
build should represent an exact single payload.

### sources are fetched and verified automatically

each package must define its source location and checksum. the build will fail
if the fetched source does not match. sources may be pre-seeded and/or cached
as desired.

### self-cleaning

build and staging dirs are wiped after use, and any previous version of a
cached result is removed following a successful build. automated builders
should be able to build each revision and store the results with no further
intervention.
