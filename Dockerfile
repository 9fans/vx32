FROM i386/ubuntu:devel

RUN apt-get -y update
RUN apt-get install -y libx11-dev \
	libxext-dev \
	libc6-dev \
	gcc
RUN apt-get install make

COPY . /app
WORKDIR /app

RUN cd src; make
