include config.mk

OBJ = sinit.o
BIN = sinit

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

$(OBJ):

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

dist: clean
	mkdir -p sinit-$(VERSION)
	cp LICENSE Makefile README config.def.h config.mk sinit.c sinit-$(VERSION)
	tar -cf sinit-$(VERSION).tar sinit-$(VERSION)
	gzip sinit-$(VERSION).tar
	rm -rf sinit-$(VERSION)

clean:
	rm -f $(BIN) $(OBJ) sinit-$(VERSION).tar.gz

.PHONY:
	all install uninstall dist clean
