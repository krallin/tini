include config.mk

OBJ = tini.o
BIN = tini

all: $(BIN) $(BIN)-static

$(BIN): $(OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BIN)-static: $(OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -static -o $@ $<

$(OBJ):

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

dist: clean
	mkdir -p tini-$(VERSION)
	cp LICENSE Makefile README config.def.h config.mk tini.c tini-$(VERSION)
	tar -cf tini-$(VERSION).tar tini-$(VERSION)
	gzip tini-$(VERSION).tar
	rm -rf tini-$(VERSION)

clean:
	rm -f $(BIN) $(OBJ) tini-$(VERSION).tar.gz

.PHONY:
	all install uninstall dist clean check
