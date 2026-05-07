#!/bin/bash


pattern="${1}"

docker compose run --workdir=/home/hercuser/Hercules -it --rm builder make sql plugins -j

docker compose down && docker compose up -d

sleep 2

watch "docker compose logs | grep -i ${pattern}"