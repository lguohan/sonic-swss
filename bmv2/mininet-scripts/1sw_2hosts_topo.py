#!/usr/bin/python

from mininet.cli import CLI
from mininet.log import setLogLevel
from mininet.net import Mininet, VERSION
from distutils.version import StrictVersion
from p4_mininet import P4DockerSwitch
import sys
import time

def main():
    net = Mininet( controller=None )
    h1 = net.addHost( 'h1', ip = '172.16.101.5/24', mac = '00:04:00:00:00:02' )
    h2 = net.addHost( 'h2', ip = '172.16.102.5/24', mac = '00:05:00:00:00:02' )

    sw1 = net.addSwitch( 'sw1', cls=P4DockerSwitch,
                        target_name="p4sonicswitch",
                        start_program="/bin/bash")

    # add links
    if StrictVersion(VERSION) <= StrictVersion('2.2.0') :
        net.addLink( sw1, h1, port1 = 1 )
        net.addLink( sw1, h2, port1 = 2 )
    else:
        net.addLink( sw1, h1, port1 = 1, fast=False )
        net.addLink( sw1, h2, port1 = 2, fast=False )

    sw1.cpFile('run_bm_sw1.sh', '/sonic-swss/bmv2/run_bm.sh')
    sw1.cpFile('1sw_l3_static_config.sh', '/scripts/l3_static_config.sh')

    sw1.execProgram("/scripts/l3_static_config.sh")
    sw1.execProgram("/scripts/startup.sh")
    net.start()

    # wait for 10 seconds for model(s) to initialize
    time.sleep( 10 )

    # hosts configuration - ipv4
    h1.setARP( ip = '172.16.101.1', mac = '00:01:04:06:08:03' )
    h2.setARP( ip = '172.16.102.1', mac = '00:01:04:06:08:03' )

    h1.setDefaultRoute( 'via 172.16.101.1' )
    h2.setDefaultRoute( 'via 172.16.102.1' )


    hosts = net.hosts
    print hosts
    for host in hosts:
        print "ARP ENTRIES ON HOST"
        print host.cmd('arp -n')
        print "HOST ROUTES"
        print host.cmd('route')
        print "HOST INTERFACE LIST"
        intfList = host.intfNames()
        print intfList


    #sw1.cmd( 'service quagga start')
    #sw2.cmd( 'service quagga start')

    result = 0

    CLI( net )

    net.stop()

if __name__ == '__main__':
    args = sys.argv
    setLogLevel( 'info' )
    main()
