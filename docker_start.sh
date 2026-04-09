#!/bin/bash

cd /home/hercuser/Hercules

if [[ ! -z ${COMPILE_SERVER} ]] && [[ $COMPILE_SERVER == "true" ]]; then
    echo clean and compiling server
    if [ -f "Makefile" ]; then
        make clean
    fi
    ./configure --enable-packetver=${PACKETVER}
    make sql -j
fi

exec ./${SERVER_TYPE}

echo "Now Started Athena."
# ./athena-start start