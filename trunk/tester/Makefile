MAINS := tstChunks tstSearch
BUILD_DIRECTORY := build
MAINS_FILES := $(addprefix $(BUILD_DIRECTORY)/, $(MAINS))
MAINS_OBJ := $(addsuffix .o,$(MAINS_FILES))
HEADER_FILES := $(wildcard *.h)
ALL_OBJ := $(addprefix $(BUILD_DIRECTORY)/,$(patsubst %.cpp,%.o,$(wildcard *.cpp)))
CLASS_OBJ := $(filter-out $(MAINS_OBJ), $(ALL_OBJ))
INCLUDE_DIRECTORY := ../ahocorasick
LINK_DIRECTORY := ../ahocorasick/build
LINK_LIBRARY := ahocorasick
LINK_TARGET := $(LINK_DIRECTORY)/lib$(LINK_LIBRARY).a
COMPILER := g++

ifeq ($(wildcard $(LINK_TARGET)),) 
$(error 'Please go to ../../ahocorasick directory and complie it first.')
endif

all: $(BUILD_DIRECTORY) $(MAINS_FILES)

$(MAINS_FILES): $(CLASS_OBJ) $(MAINS_OBJ)
	$(COMPILER) -o $@ $@.o $(CLASS_OBJ) -L$(LINK_DIRECTORY) -l$(LINK_LIBRARY) -Wall

$(BUILD_DIRECTORY)/%.o: %.cpp $(HEADER_FILES)
	$(COMPILER) -o $@ -c $< $(CFLAGS) -I$(INCLUDE_DIRECTORY) -Wall

$(BUILD_DIRECTORY):
	@mkdir -p $(BUILD_DIRECTORY)

clean:
	rm -rf $(BUILD_DIRECTORY)
