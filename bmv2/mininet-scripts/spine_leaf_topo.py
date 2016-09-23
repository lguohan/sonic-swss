#!/usr/bin/python

# Copyright 2013-present Barefoot Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

##############################################################################
# Topology with two spine two leaf switches and four hosts with BGP
#
#             sw1                            sw2
#   172.16.10.2 | 172.16.20.2     172.18.10.2 | 172.18.20.2
#      |                \             /             |
#      |                 \           /              |
#      |                  \         /               |
#      |                   \       /                |
#      |                    \     /                 |
#      |                     \   /                  |
#      |                      \ /                   |
#      |                      / \                   |
#      |                     /   \                  |
#      |                    /     \                 |
#      |                   /       \                |
#      |                  /         \               |
#      |                 /           \              |
#  172.16.10.1 | 172.18.10.1       172.16.20.1 | 172.18.20.1
#             sw3                            sw4
#  172.16.101.1 | 172.18.101.1     172.16.102.1 | 172.18.102.1
#      |                |                |              |
#      h1               h2               h3             h4
#  172.16.101.5   172.18.101.5     172.16.102.5   172.18.102.5
#
##############################################################################

from mininet.net import Mininet, VERSION
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from distutils.version import StrictVersion
from p4_mininet import P4DockerSwitch
from time import sleep
import sys

def main(cli=0):
    net = Mininet( controller = None )

    # add hosts
    h1 = net.addHost( 'h1', ip = '172.16.101.5/24', mac = '00:04:00:00:00:02' )
    h2 = net.addHost( 'h2', ip = '172.18.101.5/24', mac = '00:04:00:00:00:03' )
    h3 = net.addHost( 'h3', ip = '172.16.102.5/24', mac = '00:05:00:00:00:02' )
    h4 = net.addHost( 'h4', ip = '172.18.102.5/24', mac = '00:05:00:00:00:03' )

    # add switch 1 - spine 1
    sw1 = net.addSwitch( 'sw1', target_name = "p4sonicswitch",
            cls = P4DockerSwitch, config_fs = 'configs/sw1/l3_bgp',
            pcap_dump = True, start_program='/bin/bash')

    # add switch 2 - spine 2
    sw2 = net.addSwitch( 'sw2', target_name = "p4sonicswitch",
            cls = P4DockerSwitch, config_fs = 'configs/sw2/l3_bgp',
            pcap_dump = True, start_program='/bin/bash')

    # add switch 3 - leaf 1
    sw3 = net.addSwitch( 'sw3', target_name = "p4sonicswitch",
            cls = P4DockerSwitch, config_fs = 'configs/sw3/l3_bgp',
            pcap_dump = True, start_program='/bin/bash')

    # add switch 4 - leaf 2
    sw4 = net.addSwitch( 'sw4', target_name = "p4sonicswitch",
            cls = P4DockerSwitch, config_fs = 'configs/sw4/l3_bgp',
            pcap_dump = True, start_program='/bin/bash')

    # add links
    if StrictVersion(VERSION) <= StrictVersion('2.2.0') :
        net.addLink( sw3, h1, port1 = 1 )
        net.addLink( sw3, h2, port1 = 2 )
        net.addLink( sw1, sw3, port1 = 1, port2 = 3 )
        net.addLink( sw2, sw3, port1 = 1, port2 = 4 )
        net.addLink( sw4, h3, port1 = 1 )
        net.addLink( sw4, h4, port1 = 2 )
        net.addLink( sw1, sw4, port1 = 2, port2 = 3 )
        net.addLink( sw2, sw4, port1 = 2, port2 = 4 )
    else:
        net.addLink( sw3, h1, port1 = 1 , fast=False )
        net.addLink( sw3, h2, port1 = 2 , fast=False )
        net.addLink( sw1, sw3, port1 = 1, port2 = 3 , fast=False )
        net.addLink( sw2, sw3, port1 = 1, port2 = 4 , fast=False )
        net.addLink( sw4, h3, port1 = 1 , fast=False )
        net.addLink( sw4, h4, port1 = 2 , fast=False )
        net.addLink( sw1, sw4, port1 = 2, port2 = 3 , fast=False )
        net.addLink( sw2, sw4, port1 = 2, port2 = 4 , fast=False )

    sw1.cpFile('run_bm_sw1.sh', '/sonic-swss/bmv2/run_bm.sh')
    sw1.execProgram('/scripts/startup.sh', args='-m  00:00:01:00:00:01')
    sw1.execProgram("/configs/startup_config.sh")

    sw2.cpFile('run_bm_sw2.sh', '/sonic-swss/bmv2/run_bm.sh')
    sw2.execProgram('/scripts/startup.sh', args='-m 00:00:01:00:00:02')
    sw2.execProgram("/configs/startup_config.sh")

    sw3.cpFile('run_bm_sw3.sh', '/sonic-swss/bmv2/run_bm.sh')
    sw3.execProgram('/scripts/startup.sh', args='-m 00:00:01:00:00:03')
    sw3.execProgram("/configs/startup_config.sh")

    sw4.cpFile('run_bm_sw4.sh', '/sonic-swss/bmv2/run_bm.sh')
    sw4.execProgram('/scripts/startup.sh', args='-m 00:00:01:00:00:04')
    sw4.execProgram("/configs/startup_config.sh")

    net.start()

    # hosts configuration - ipv4
    h1.setDefaultRoute( 'via 172.16.101.1' )
    h2.setDefaultRoute( 'via 172.18.101.1' )
    h3.setDefaultRoute( 'via 172.16.102.1' )
    h4.setDefaultRoute( 'via 172.18.102.1' )

    sw1.cmd( 'service quagga start')
    sw2.cmd( 'service quagga start')
    sw3.cmd( 'service quagga start')
    sw4.cmd( 'service quagga start')

    result = 0

    if cli:
        CLI(net)
    else:
        sleep(90)

        node_values = net.values()
        print node_values

        hosts = net.hosts
        print hosts

        # ping hosts
        print "PING BETWEEN THE HOSTS"
        result = net.ping(hosts,30)

        if result != 0:
            print "PING FAILED BETWEEN HOSTS %s"  % (hosts)
        else:
            print "PING SUCCESSFUL!!!"

        # print host arp table & routes
        for host in hosts:
            print "ARP ENTRIES ON HOST"
            print host.cmd('arp -n')
            print "HOST ROUTES"
            print host.cmd('route')
            print "HOST INTERFACE LIST"
            intfList = host.intfNames()
            print intfList

    net.stop()
    return result

if __name__ == '__main__':
    args = sys.argv
    setLogLevel( 'info' )
    cli = 0
    if "--cli" in args:
        cli = 1
    main(cli)
