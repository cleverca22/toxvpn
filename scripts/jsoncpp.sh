#!/bin/bash

if [ $TRAVIS_OS_NAME == osx ]; then
  brew tap cuber/homebrew-jsoncpp
  sudo brew install jsoncpp
fi
