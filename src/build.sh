#!/bin/bash

SRC_PATH="."

APP_PATH="/home/yecid/rerobapp"

DOCKER_PATH="$APP_PATH/docker"
USER_DOCKER_PATH="$DOCKER_PATH/user"
BUILD_DOCKER_PATH="$DOCKER_PATH/user/build"
SRC_DOCKER_PATH="$DOCKER_PATH/user/src"

rm -r "$SRC_DOCKER_PATH"

mkdir -p "$USER_DOCKER_PATH"
mkdir -p "$BUILD_DOCKER_PATH"
mkdir -p "$SRC_DOCKER_PATH"

rsync -aruh "$SRC_PATH/" "$SRC_DOCKER_PATH"

docker run -it --rm --user $(id -u):$(id -g) \
    --env="DISPLAY" \
    --env="QT_X11_NO_MITSHM=1" \
    --env="TERM=xterm-256color" \
    --network="host" \
    --ipc="host" \
    --privileged \
    --volume="/etc/group:/etc/group:ro" \
    --volume="/etc/passwd:/etc/passwd:ro" \
    --volume="/etc/shadow:/etc/shadow:ro" \
    --volume="/etc/sudoers.d:/etc/sudoers.d:ro" \
    --volume="/tmp/.X11-unix:/tmp/.X11-unix:rw" \
    --volume="$USER_DOCKER_PATH:/home/$USER:rw" \
    --volume="$BUILD_DOCKER_PATH:/home/$USER/build:rw" \
    --name beaglebone_c --workdir="/home/$USER" \
    beaglebone2023:lasted "./src/task.sh"

rsync -aruvh  $BUILD_DOCKER_PATH debian@192.168.6.2:/home/debian/
rsync -aruvh  exoapp/res/ debian@192.168.6.2:/home/debian/res

# read -p "Press enter to continue . . ."

