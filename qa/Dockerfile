FROM        ubuntu:18.04

COPY        . /atomicDEX

ARG         TAG=beta-2.0.1979
ARG         CHASH=cc20999bc

ENV         LINK=https://github.com/KomodoPlatform/atomicDEX-API/releases/download/${TAG}/mm2-${CHASH}-Linux-Debug.zip

RUN         apt-get update && \
            apt-get install -y \
            dnsutils \
            curl \
            libssl-dev \
            libcurl4-openssl-dev \
            python3.8 \
            python3-pip \
            zip \
            unzip \
            wget \
            python3.8-dev \
            python3-setuptools && \
            python3.8 -m pip install pip --upgrade && \
            python3.8 -m pip install setuptools wheel pycurl ujson && \
            python3.8 -m pip install pytest slick-bitcoinrpc && \
            mkdir /atomicDEX/mmbin && \
            cd /atomicDEX/mmbin && \
            wget ${LINK} && \
            export ARCHIVE=`ls | grep mm2` && \
            unzip ${ARCHIVE}

ENV         PATH=/atomicDEX/mmbin/mm2:${PATH}

WORKDIR     /atomicDEX/tests

CMD         mm2