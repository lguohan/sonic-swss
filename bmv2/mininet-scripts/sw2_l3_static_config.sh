#!/bin/bash

ip tuntap add dev Ethernet0 mode tap
ip tuntap add dev Ethernet1 mode tap
ip link set Ethernet0 up
ip link set Ethernet1 up

ip link set dev Ethernet0 address 00:01:04:06:08:03
ip link set dev Ethernet1 address 00:01:04:06:08:03
ip address add 172.16.102.1/24 broadcast + dev Ethernet0
ip address add 172.16.10.2/24 broadcast + dev Ethernet1
ip neigh add 172.16.102.5 lladdr 00:05:00:00:00:02 dev Ethernet0
ip neigh add 172.16.10.1 lladdr 00:01:04:06:08:03 dev Ethernet1
ip route add 172.16.101/24 nexthop via 172.16.10.1
