INCLUDE_DIRECTORY := ../ahocorasick
LIBRARY_DIRECTORY := ../ahocorasick/build
LINK_LIBRARY := ahocorasick
BUILD_DIRECTORY := build

ifeq ($(wildcard $(LIBRARY_DIRECTORY)/lib$(LINK_LIBRARY).a),) 
all:;@echo 'Please go to ../ahocorasick directory and complie it first.'
else
all: example2
endif

example2: mkbuilddir example2.o
	cc -o $(BUILD_DIRECTORY)/example2 $(BUILD_DIRECTORY)/example2.o -l$(LINK_LIBRARY) -L$(LIBRARY_DIRECTORY)

example2.o: example2.c
	cc -o $(BUILD_DIRECTORY)/example2.o -I$(INCLUDE_DIRECTORY) -Wall -c example2.c

mkbuilddir:
	mkdir -p $(BUILD_DIRECTORY)

clean:
	rm -rf $(BUILD_DIRECTORY)
