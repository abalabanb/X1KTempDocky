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
VERSION = 1

# Source code files used in this project
# Add any additional files to this line

SRCS = init.c docky.c cfg.c readtemp.c

# -------------------------------------------------------------
# Nothing should need changing below this line

OBJS = $(SRCS:.c=.o)
# Rules for building
$(TARGET): $(OBJS)
	$(CC) $(LINK) -nostdlib -o $(TARGET) $(OBJS) $(LIBS)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS)

.PHONY: distclean
distclean:
	$(RM) $(OBJS)

.PHONY: install
install: $(TARGET)
	$(CP) $(TARGET) /Utilities/Dockies/

.PHONY: revision
revision:
	bumprev $(VERSION) $(TARGET)

init.c: dockybase.h
docky.c: dockybase.h
init.c: dockybase.h
readtemp.c: dockybase.h

