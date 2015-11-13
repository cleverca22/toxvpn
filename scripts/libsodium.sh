#!/bin/bash
if [ ! -d "$HOME/libsodium/lib" ]; then
  mkdir build
  cd build
  git clone git://github.com/jedisct1/libsodium.git > /dev/null
  cd libsodium
  git checkout tags/1.0.0 > /dev/null
  ./autogen.sh > /dev/null
  ./configure --prefix=${HOME}/libsodium/
  make check -j3 > /dev/null
  make install
else
  echo 'Using cached directory.';
fi
