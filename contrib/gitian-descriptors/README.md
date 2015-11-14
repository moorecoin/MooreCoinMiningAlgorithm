### gavin's notes on getting gitian builds up and running using kvm:###

these instructions distilled from:
[  https://help.ubuntu.com/community/kvm/installation](  https://help.ubuntu.com/community/kvm/installation)
... see there for complete details.

you need the right hardware: you need a 64-bit-capable cpu with hardware virtualization support (intel vt-x or amd-v). not all modern cpus support hardware virtualization.

you probably need to enable hardware virtualization in your machine's bios.

you need to be running a recent version of 64-bit-ubuntu, and you need to install several prerequisites:

	sudo apt-get install ruby apache2 git apt-cacher-ng python-vm-builder qemu-kvm

sanity checks:

	sudo service apt-cacher-ng status  # should return apt-cacher-ng is running
	ls -l /dev/kvm   # should show a /dev/kvm device


once you've got the right hardware and software:

    git clone git://github.com/bitcoin/bitcoin.git
    git clone git://github.com/devrandom/gitian-builder.git
    mkdir gitian-builder/inputs
    cd gitian-builder/inputs

    # create base images
    cd gitian-builder
    bin/make-base-vm --suite precise --arch amd64
    cd ..

    # get inputs (see doc/release-process.md for exact inputs needed and where to get them)
    ...

    # for further build instructions see doc/release-notes.md
    ...

---------------------

`gitian-builder` now also supports building using lxc. see
[  https://help.ubuntu.com/12.04/serverguide/lxc.html](  https://help.ubuntu.com/12.04/serverguide/lxc.html)
... for how to get lxc up and running under ubuntu.

if your main machine is a 64-bit mac or pc with a few gigabytes of memory
and at least 10 gigabytes of free disk space, you can `gitian-build` using
lxc running inside a virtual machine.

here's a description of gavin's setup on osx 10.6:

1. download and install virtualbox from [https://www.virtualbox.org/](https://www.virtualbox.org/)

2. download the 64-bit ubuntu desktop 12.04 lts .iso cd image from
   [http://www.ubuntu.com/](http://www.ubuntu.com/)

3. run virtualbox and create a new virtual machine, using the ubuntu .iso (see the [virtualbox documentation](https://www.virtualbox.org/wiki/documentation) for details). create it with at least 2 gigabytes of memory and a disk that is at least 20 gigabytes big.

4. inside the running ubuntu desktop, install:

	sudo apt-get install debootstrap lxc ruby apache2 git apt-cacher-ng python-vm-builder

5. still inside ubuntu, tell gitian-builder to use lxc, then follow the "once you've got the right hardware and software" instructions above:

	export use_lxc=1
	git clone git://github.com/bitcoin/bitcoin.git
	... etc
