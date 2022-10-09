all:
	make -C libtimidity-0.1.0/src
	make -C libtimidity-0.1.0/src install
	make -C MeBootStart
	cp MeBootStart/MeBootStart.prx .
	make -f Makefile.audio
	make -f Makefile.audio install

samples:
	@make -C "New sample Wma"
	@make -C "New sample Aa3"

clean:
	make -f Makefile.audio clean
