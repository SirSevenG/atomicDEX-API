FROM        ubuntu:18.04

COPY        docker /atomicDEX/mmbin

ARG         TAG=beta-2.0.1979
ARG         CHASH=cc20999bc
ARG         REPO=KomodoPlatform/atomicDEX-API

ENV         LINK=https://github.com/${REPO}/releases/download/${TAG}/mm2-${CHASH}-Linux-Debug.zip

RUN         apt-get update && \
            apt-get install -y --no-install-recommends \
            dnsutils \
            curl \
            libssl-dev \
            libcurl4-openssl-dev \
            zip \
            wget \
            unzip && \
            apt-get clean && \
            rm -rf /var/lib/apt/lists/* && \
            cd /atomicDEX/mmbin && \
            wget ${LINK} && \
            export ARCHIVE=$(echo ./mm2*) && \
            unzip ${ARCHIVE} && \
            export PATH=/atomicDEX/mmbin/mm2:${PATH}

WORKDIR     /atomicDEX/mmbin

CMD         mm2