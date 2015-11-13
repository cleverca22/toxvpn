#!/bin/bash

if [ $TRAVIS_OS_NAME == osx ]; then
  brew tap cuber/homebrew-jsoncpp
  brew unlink json-c
  brew install jsoncpp
fi
