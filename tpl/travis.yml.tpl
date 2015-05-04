language: c
compiler:
- gcc
- clang
script: ./ci/run_build.sh

sudo: false

deploy:
  provider: releases
  api_key:
    secure: Yk90ANpSPv1iJy8QDXCPwfaSmEr/WIJ3bzhQ6X8JvZjfrwTosbh0HrUzQyeac3nyvNwj7YJRssolOFc21IBKPpCFTZqYxSkuLPU6ysG4HGHgN6YJhOMm4mG4KKJ6741q3DJendhZpalBhCEi+NcZK/PCSD97Vl4OqRjBUged0fs=
  file:
    - "./dist/tini"
    - "./dist/tini_@tini_VERSION_MAJOR@.@tini_VERSION_MINOR@.@tini_VERSION_PATCH@.deb"
    - "./dist/tini_@tini_VERSION_MAJOR@.@tini_VERSION_MINOR@.@tini_VERSION_PATCH@.rpm"
  on:
    repo: krallin/tini
    tags: true
    condition: "$CC = gcc"
