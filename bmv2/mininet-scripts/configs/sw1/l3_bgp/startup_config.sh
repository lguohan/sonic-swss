#!/bin/bash

ip tuntap add dev Ethernet0 mode tap
ip tuntap add dev Ethernet1 mode tap
ip link set Ethernet0 up
ip link set Ethernet1 up

ip link set dev Ethernet0 address 00:01:04:06:08:03
ip link set dev Ethernet1 address 00:01:04:06:08:03

ip address add 172.16.10.2/24 broadcast + dev Ethernet0
ip address add 172.16.20.2/24 broadcast + dev Ethernet1

cp /configs/quagga/* /etc/quagga/
chown quagga.quagga /etc/quagga/*
