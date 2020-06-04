FROM        ubuntu:bionic
LABEL       maintainer="SirSevenG <ottonseven@gmail.com>"

ARG         DEBIAN_FRONTEND=noninteractive

COPY        docker/ /workdir

ENV         PACKS="python3.8 \
            wget \
            nano \
            python3-setuptools \
            python3-dev \
            python3-pip \
            curl \
            libcurl4-openssl-dev \
            libssl-dev"

RUN         apt-get update && \
            apt-get -y install -q $PACKS && \
            python3 -m pip install --upgrade pip && \
            python3 -m pip install -r /workdir/reqs.txt

USER        swapper
WORKDIR     /workdir

CMD ["python3", "-m", "pytest", "saturation.py", "-s", "-vv"]