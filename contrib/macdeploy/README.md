### macdeploy ###

for snow leopard (which uses [python 2.6](http://www.python.org/download/releases/2.6/)), you will need the param_parser package:
	
	sudo easy_install argparse

this script should not be run manually, instead, after building as usual:

	make deploy

during the process, the disk image window will pop up briefly where the fancy
settings are applied. this is normal, please do not interfere.

when finished, it will produce `bitcoin-core.dmg`.

