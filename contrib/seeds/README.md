### seeds ###

utility to generate the seeds.txt list that is compiled into the client
(see [src/chainparamsseeds.h](/src/chainparamsseeds.h) and [share/seeds](/share/seeds)).

the 512 seeds compiled into the 0.10 release were created from sipa's dns seed data, like this:

	curl -s http://bitcoin.sipa.be/seeds.txt | makeseeds.py
