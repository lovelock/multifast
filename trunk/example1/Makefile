INCLUDE_DIRECTORY := ../ahocorasick
LIBRARY_DIRECTORY := ../ahocorasick/build
LINK_LIBRARY := ahocorasick
BUILD_DIRECTORY := build

ifeq ($(wildcard $(LIBRARY_DIRECTORY)/lib$(LINK_LIBRARY).a),) 
all:;@echo 'Please go to ../ahocorasick directory and complie it first.'
else
all: example1
endif

example1: mkbuilddir example1.o
	cc -o $(BUILD_DIRECTORY)/example1 $(BUILD_DIRECTORY)/example1.o -l$(LINK_LIBRARY) -L$(LIBRARY_DIRECTORY)

example1.o: example1.c
	cc -o $(BUILD_DIRECTORY)/example1.o -I$(INCLUDE_DIRECTORY) -Wall -c example1.c

mkbuilddir:
	mkdir -p $(BUILD_DIRECTORY)

clean:
	rm -rf $(BUILD_DIRECTORY)
