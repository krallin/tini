VERSION = 0.1.0

# paths
PREFIX=/usr/local

CC = cc
LD = $(CC)
CPPFLAGS=-D_FORTIFY_SOURCE=2
CFLAGS=-std=gnu99 -Wextra -Wall -pedantic -O2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security
LDFLAGS=-Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-s
