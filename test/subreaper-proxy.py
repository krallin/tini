#!/usr/bin/env python
#coding:utf-8
import os
import sys

import prctl


def main():
    args = sys.argv[1:]

    print "subreaper-proxy: running '%s'" % (" ".join(args))

    prctl.set_child_subreaper(1)
    os.execv(args[0], args)


if __name__ == '__main__':
    main()
