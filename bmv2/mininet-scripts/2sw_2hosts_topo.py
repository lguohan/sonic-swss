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
# Topology with two switches and two hosts with static routes
#
#       2ffe:0101::/64          2ffe:0010::/64         2ffe:0102::/64
#       172.16.101.0/24         172.16.10.0/24         172.16.102.0./24
#  h1 ------------------- sw1 ------------------ sw2------- -------------h2
#     .5               .1     .1               .2   .1                  .5
##############################################################################

from mininet.net import Mininet, VERSION
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from distutils.version import StrictVersion
from p4_mininet import P4DockerSwitch
from time import sleep
import sys
from mininet.link import Intf

def main():
    net = Mininet( controller = None )

    # add hosts
    h1 = net.addHost( 'h1', ip = '172.16.101.5/24', mac = '00:04:00:00:00:02' )
    h2 = net.addHost( 'h2', ip = '172.16.102.5/24', mac = '00:05:00:00:00:02' )

    # add switch 1
    sw1 = net.addSwitch( 'sw1', target_name = "p4sonicswitch",
            cls = P4DockerSwitch, start_program = '/bin/bash',
            pcap_dump = True )

    # add switch 2
    sw2 = net.addSwitch( 'sw2', target_name = "p4sonicswitch",
            cls = P4DockerSwitch, start_program = '/bin/bash',
            pcap_dump = True )

   
    # add links
    if StrictVersion(VERSION) <= StrictVersion('2.2.0') :
        net.addLink( sw1, h1, port1 = 1 )
        net.addLink( sw1, sw2, port1 = 2, port2 = 2 )
        net.addLink( sw2, h2, port1 = 1 )
    else:
        net.addLink( sw1, h1, port1 = 1, fast=False )
        net.addLink( sw1, sw2, port1 = 2, port2 = 2, fast=False )
        net.addLink( sw2, h2, port1 = 1, fast=False )

    sw1.cpFile('run_bm_sw1.sh', '/sonic-swss/bmv2/run_bm.sh')
    sw1.cpFile('sw1_l3_static_config.sh', '/scripts/l3_static_config.sh')

    sw2.cpFile('run_bm_sw2.sh', '/sonic-swss/bmv2/run_bm.sh')
    sw2.cpFile('sw2_l3_static_config.sh', '/scripts/l3_static_config.sh')

    sw1.execProgram("/scripts/l3_static_config.sh")
    sw2.execProgram("/scripts/l3_static_config.sh")

    sw1.execProgram("/scripts/startup.sh")
    sw2.execProgram("/scripts/startup.sh")

    net.start()

    # hosts configuration - ipv4
    h1.setARP( ip = '172.16.101.1', mac = '00:01:04:06:08:03' )
    h2.setARP( ip = '172.16.102.1', mac = '00:01:04:06:08:03' )

    h1.setDefaultRoute( 'via 172.16.101.1' )
    h2.setDefaultRoute( 'via 172.16.102.1' )

    CLI( net )

    net.stop()


if __name__ == '__main__':
    setLogLevel( 'info' )
    main()
