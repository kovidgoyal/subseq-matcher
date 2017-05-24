#
# Makefile
# Kovid Goyal, 2017-05-22 11:19
#

all: 
	python setup.py

show-help: 
	python setup.py show-help

test:
	python test.py

clean:
	rm -rf $(BUILD)


# vim:ft=make
