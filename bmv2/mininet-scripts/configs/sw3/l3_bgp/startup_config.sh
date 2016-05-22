#!/bin/bash

stty -echo; set +m

ip tuntap add dev Ethernet0 mode tap
ip tuntap add dev Ethernet1 mode tap
ip tuntap add dev Ethernet2 mode tap
ip tuntap add dev Ethernet3 mode tap
ip link set Ethernet0 up
ip link set Ethernet1 up
ip link set Ethernet3 up
ip link set Ethernet4 up

ip link set dev Ethernet0 address 00:01:04:06:08:03
ip link set dev Ethernet1 address 00:01:04:06:08:03
ip link set dev Ethernet2 address 00:01:04:06:08:03
ip link set dev Ethernet3 address 00:01:04:06:08:03

ip address add 172.16.101.1/24 broadcast + dev Ethernet0
ip address add 172.17.101.1/24 broadcast + dev Ethernet1
ip address add 172.16.10.1/24 broadcast + dev Ethernet2
ip address add 172.17.10.1/24 broadcast + dev Ethernet3

cp /configs/quagga/* /etc/quagga/
chown quagga.quagga /etc/quagga/*
