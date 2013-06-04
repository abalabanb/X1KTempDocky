###
### Makefile to build X1kTemp.docky
### Output is created in the current directory
###



CC     = gcc
CP	   = copy clone
RM     = delete

# Change these as required
OPTIMIZE = -O4
DEBUG = -DNDEBUG -g
CFLAGS = $(OPTIMIZE) $(DEBUG) -I. -Iinclude

# Flags passed to gcc during linking
LINK = $(OPTIMIZE) $(DEBUG)

# Name of the "thing" to build
TARGET = X1kTemp.docky

# Additional linker libraries
LIBS =

# Version of the library to build
VERSION = 53

# Source code files used in this project
# Add any additional files to this line

SRCS = init.c docky.c cfg.c readtemp.c locale_support.c

CTS = X1kTemp_fr.ct X1kTemp_de.ct X1kTemp_gr.ct X1kTemp_it.ct X1kTemp_fi.ct

# -------------------------------------------------------------
# Nothing should need changing below this line

OBJS = $(SRCS:.c=.o)

CATALOGS = $(CTS:.ct=.catalog)

# Rules for building
all: $(TARGET) $(CATALOGS)

$(TARGET): $(OBJS)
	$(CC) $(LINK) -nostdlib -o $(TARGET) $(OBJS) $(LIBS)

locale.h: X1kTemp.cd
	Catcomp $^ CFILE $@ NOCODE

%.catalog: X1kTemp.cd %.ct
	CatComp $^ CATALOG $(amigapath $@)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS)

.PHONY: release
release:
	Copy "X1kTemp Setup/" T:X1kTemp CLONE ALL FOLLOWLINKS COPYLINKS
	Copy "X1kTemp Setup.info" T:X1kTemp.info
	Delete t:X1kTemp/.svn all
	Rename t:X1kTemp/AutoInstall T:

.PHONY: distclean
distclean:
	$(RM) $(OBJS)

.PHONY: install
install: $(TARGET)
	$(CP) $(TARGET) /Utilities/Dockies/

.PHONY: revision
revision:
	bumprev $(VERSION) $(TARGET)

init.o: dockybase.h X1kTemp.docky_rev.h
docky.o: dockybase.h X1kTemp.docky_rev.h locale_support.h
init.o: dockybase.h X1kTemp.docky_rev.h
readtemp.o: dockybase.h X1kTemp.docky_rev.h
locale_support.o: locale_support.h locale.h
locale_support.h: locale.h
