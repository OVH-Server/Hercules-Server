FROM ubuntu:24.04
RUN apt update && apt install git make gcc mysql-server libmysqlclient-dev zlib1g-dev libpcre3-dev screen -y
RUN useradd hercuser
RUN mkdir -p /home/hercuser && chown -R hercuser:hercuser /home/hercuser
USER hercuser
# COPY ../ /home/hercuser/Hercules
# WORKDIR /home/hercuser/Hercules
# RUN pwd && ls -la && ./configure
# RUN make sql
