FROM ubuntu:24.04
RUN useradd hercuser
RUN mkdir -p /home/hercuser && chown -R hercuser:hercuser /home/hercuser
COPY ./ /home/hercuser/Hercules
WORKDIR /home/hercuser/Hercules
