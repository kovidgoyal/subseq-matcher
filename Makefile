#
# Makefile
# Kovid Goyal, 2017-05-22 11:19
#

BUILD=build
EXE=subseq-matcher

all: 
	python setup.py

show-help: 
	python setup.py show-help

test:
	python test.py

clean:
	rm -rf $(BUILD)

install:
	install -s -D build/$(EXE) $(DESTDIR)/usr/bin/$(EXE)
	
uninstall:
	rm -f $(DESTDIR)/usr/bin/$(EXE)

# vim:ft=make
