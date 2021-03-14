#############################################
##                                         ##
##    Copyright (C) 2019-2021 Julian Uy    ##
##  https://sites.google.com/site/awertyb  ##
##                                         ##
##   See details of license at "LICENSE"   ##
##                                         ##
#############################################

TOOL_TRIPLET_PREFIX ?= i686-w64-mingw32-
CC := $(TOOL_TRIPLET_PREFIX)gcc
CXX := $(TOOL_TRIPLET_PREFIX)g++
AR := $(TOOL_TRIPLET_PREFIX)ar
WINDRES := $(TOOL_TRIPLET_PREFIX)windres
STRIP := $(TOOL_TRIPLET_PREFIX)strip
GIT_TAG := $(shell git describe --abbrev=0 --tags)
INCFLAGS += -I. -I.. -Iexternal/libtlg
ALLSRCFLAGS += $(INCFLAGS) -DGIT_TAG=\"$(GIT_TAG)\"
CFLAGS += -O3 -flto
CFLAGS += $(ALLSRCFLAGS) -Wall -Wno-unused-value -Wno-format -DNDEBUG -DWIN32 -D_WIN32 -D_WINDOWS 
CFLAGS += -D_USRDLL -DUNICODE -D_UNICODE 
CXXFLAGS += $(CFLAGS) -fpermissive
WINDRESFLAGS += $(ALLSRCFLAGS) --codepage=65001
LDFLAGS += -static -static-libstdc++ -static-libgcc -shared -Wl,--kill-at
LDLIBS +=

%.o: %.c
	@printf '\t%s %s\n' CC $<
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cpp
	@printf '\t%s %s\n' CXX $<
	$(CXX) -c $(CXXFLAGS) -o $@ $<

%.o: %.rc
	@printf '\t%s %s\n' WINDRES $<
	$(WINDRES) $(WINDRESFLAGS) $< $@

LIBTLG_SOURCES += external/libtlg/LoadTLG.cpp external/libtlg/handlestream.cpp external/libtlg/memstream.cpp external/libtlg/slide.cpp external/libtlg/stream.cpp external/libtlg/tvpgl.c
SOURCES := extractor.cpp spi00in.c iftlg.rc $(LIBTLG_SOURCES)
OBJECTS := $(SOURCES:.c=.o)
OBJECTS := $(OBJECTS:.cpp=.o)
OBJECTS := $(OBJECTS:.rc=.o)

BINARY ?= iftlg_unstripped.spi
BINARY_STRIPPED ?= iftlg.spi
ARCHIVE ?= iftlg.$(GIT_TAG).7z

all: $(BINARY_STRIPPED)

archive: $(ARCHIVE)

clean:
	rm -f $(OBJECTS) $(BINARY) $(BINARY_STRIPPED) $(ARCHIVE)

$(ARCHIVE): $(BINARY_STRIPPED)
	rm -f $(ARCHIVE)
	7z a $@ $^

$(BINARY_STRIPPED): $(BINARY)
	@printf '\t%s %s\n' STRIP $@
	$(STRIP) -o $@ $^

$(BINARY): $(OBJECTS) 
	@printf '\t%s %s\n' LNK $@
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
