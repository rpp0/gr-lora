#!/bin/sh

LATEST_VERSION=`git ls-remote https://github.com/rpp0/gr-lora.git | grep HEAD | cut -f 1`
DOCKER_XAUTH=/tmp/.docker.xauth
touch /tmp/.docker.xauth
xauth nlist $DISPLAY | sed -e 's/^..../ffff/' | xauth -f $DOCKER_XAUTH nmerge -
docker run -i -t --rm --privileged -e DISPLAY=$DISPLAY -e XAUTHORITY=$DOCKER_XAUTH -v /tmp/.docker.xauth:/tmp/.docker.xauth:ro -v /tmp/.X11-unix:/tmp/.X11-unix:ro -v /dev/bus/usb:/dev/bus/usb --entrypoint /bin/bash rpp0/gr-lora:$LATEST_VERSION
