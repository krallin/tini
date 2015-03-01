VERSION=0.3.0
GIT_REV=$(shell git log -n 1 --date=local --pretty=format:"%h")

# paths
PREFIX=/usr/local

CC = cc
LD = $(CC)
CPPFLAGS=-D_FORTIFY_SOURCE=2 -DTINI_VERSION="\"$(VERSION) - $(GIT_REV)\""
CFLAGS=-std=gnu99 -Wextra -Wall -pedantic -O2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security
LDFLAGS=-Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-s
