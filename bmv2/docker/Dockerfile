FROM ubuntu:16.04

RUN apt-get update
RUN apt-key adv --keyserver apt-mo.trafficmanager.net --recv-keys 417A0893
RUN 'deb http://apt-mo.trafficmanager.net/repos/sonic/ trusty main' | tee -a /etc/apt/sources.list.d/sonic.list
RUN apt-get update
RUN apt-get clean

RUN mkdir /root/.ssh/

ADD id_rsa /root/.ssh/id_rsa
ADD id_rsa.pub /root/.ssh/id_rsa.pub
ADD authorized_keys /root/.ssh/authorized_keys
ADD known_hosts /root/.ssh/known_hosts

RUN mkdir /scripts
ADD startup.sh /scripts/startup.sh

RUN apt-get install -y                  \
    autoconf                             \
    automake                             \
    bison                                \
    bridge-utils                         \
    cmake                                \
    dh-exec                              \
    doxygen                              \
    ethtool                              \
    flex                                 \
    g++                                  \
    git                                  \
    wget

RUN apt-get clean
RUN apt-get install -y                  \
    inetutils-tools                      \
    inetutils-ping                      \
    ipython                              \
    ipython-notebook

RUN apt-get clean
RUN apt-get install -y                  \
    libany-moose-perl                    \
    libboost-dev                         \
    libboost-filesystem-dev              \
    libboost-program-options-dev         \
    libboost-system-dev                  \
    libboost-test-dev                    \
    libboost-thread-dev

RUN apt-get clean
RUN apt-get install -y                  \
    libbsd-dev                           \
    libedit-dev                          \
    libevent-dev                         \
    libffi-dev                           \
    libfreetype6-dev                     \
    libgmp-dev                           \
    libhiredis0.13                       \
    libhiredis-dbg                       \
    libhiredis-dev                       \
    libjudy-dev                          \
    libnl-route-3-dev

RUN apt-get clean
RUN apt-get install -y                  \
    libpcap-dev                          \
    libpng-dev                           \
    libssl-dev                           \
    libtool                              \
    libtool-bin                          \
    libyaml-0-2                          \
    libbz2-dev                           \
    mininet                              \
    net-tools                              \
    openssl                              \
    pkg-config

RUN apt-get clean
RUN apt-get install -y                  \
    python-dev                           \
    python-dpkt                          \
    python-jsonpickle                    \
    python-imaging-tk                    \
    python-matplotlib                    \
    python-nose python-numpy             \
    python-pandas                        \
    python-pip                           \
    python-pygraph                       \
    python-pygraphviz                    \
    python-scipy                         \
    python-setuptools                    \
    python-sympy                         \
    python-yaml

RUN apt-get clean
RUN apt-get install -y                  \
    redis-sentinel                       \
    redis-server                         \
    redis-tools                          \
    rsyslog                          \
    sudo                                 \
    tcpdump                               \
    thrift-compiler                      \
    vim

RUN pip install --upgrade thrift
RUN pip install tenjin
RUN pip install ctypesgen
RUN pip install crc16

# build thrift from sources
RUN mkdir install_tmp ; \
    cd install_tmp ; \
    wget -c http://archive.apache.org/dist/thrift/0.9.2/thrift-0.9.2.tar.gz ; \
    tar zxvf thrift-0.9.2.tar.gz ; \
    cd thrift-0.9.2 ; \
    ./configure ; \
    cd test/cpp ; ln -s . .libs ; cd ../.. ; \
    make -j4 ; \
    make install ; \
    ldconfig ; \
    cd .. ; \
    git clone https://github.com/p4lang/p4-hlir.git ; \
    cd p4-hlir ; \
    python setup.py install ; \
    cd .. ; \
    apt-get remove python-scapy ; \
    git clone https://github.com/p4lang/scapy-vxlan.git ; \
    cd scapy-vxlan ; \
    python setup.py install ; \
    cd .. ; \
    git clone git@github.com:nanomsg/nanomsg.git ; \
    cd nanomsg ; \
    mkdir build ; \
    cd build ; \
    cmake .. ; \
    cmake --build . ; \
    ctest . ; \
    cmake --build . --target install ; \
    cd ../.. ; \
    cd ..

ADD quagga_0.99.24.1-2_amd64.deb /quagga_0.99.24.1-2_amd64.deb
ADD quagga-dbg_0.99.24.1-2_amd64.deb /quagga-dbg_0.99.24.1-2_amd64.deb
RUN dpkg -i quagga_0.99.24.1-2_amd64.deb
RUN dpkg -i quagga-dbg_0.99.24.1-2_amd64.deb

RUN git clone https://github.com/krambn/SAI.git
RUN git clone https://github.com/krambn/sonic-sairedis.git
RUN git clone https://github.com/krambn/sonic-swss.git
RUN git clone https://github.com/krambn/sonic-swss-common.git

RUN cd sonic-swss-common/ ; \
    ./autogen.sh ; \
    fakeroot debian/rules binary ; \
    libtool --finish /usr/lib/x86_64-linux-gnu ; \
    cd .. 

RUN dpkg -i libswsscommon_1.0.0_amd64.deb 
RUN dpkg -i libswsscommon-dev_1.0.0_amd64.deb 

RUN cd sonic-swss/bmv2 ; \
    ./getdeps.sh ; \
    cd switch/switchsai/submodules ; \
    mv SAI SAI.old ; \
    ln -s /SAI SAI ; \
    cd ../../.. ; \
    make ; \
    cd .. ; \
    ln -s /SAI/inc /usr/include/sai ; \
    cd /

RUN cd sonic-sairedis/ ; \
    ./autogen.sh ; \
    ./configure ; \
    make syncd_LDADD='-lhiredis -lswsscommon -L/sonic-swss/bmv2/install/lib -lsai -lpthread' ; \
    make install ; \
    cd ..

RUN cd sonic-swss ; \
    ./autogen.sh ; \
    ./configure ; \
    make ; \
    make install

