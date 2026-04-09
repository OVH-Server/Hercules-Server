#!/bin/bash

cd /home/hercuser/Hercules

echo compile: ${COMPILE_SERVER}

if [[ ! -z ${COMPILE_SERVER} ]] && [[ $COMPILE_SERVER == "true" ]]; then
    echo clean and compiling server
    if [ -f "Makefile" ]; then
        make clean
    fi
    if [[ ${ENABLE_RENEWAL_PACKET} == "yes" ]]; then
        ./configure --enable-packetver=${PACKETVER} --enable-packetver-re=${ENABLE_RENEWAL_PACKET}
    else
        ./configure --enable-packetver=${PACKETVER} --enable-packetver-re=${ENABLE_RENEWAL_PACKET} --disable-renewal
    fi
    make sql -j
fi

if [ ! -z ${SERVER_TYPE} ]; then
    exec ./${SERVER_TYPE}
fi

exit 0
# echo "Now Started Athena."
# ./athena-start start