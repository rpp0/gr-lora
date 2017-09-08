#!/bin/sh

LATEST_VERSION=`git ls-remote https://github.com/rpp0/gr-lora.git | grep HEAD | cut -f 1`
docker build -t rpp0/gr-lora --build-arg CACHEBUST=$LATEST_VERSION .
#docker tag rpp0/gr-lora:latest rpp0/gr-lora:$LATEST_VERSION
