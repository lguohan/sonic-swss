simple_switch --log-console  -i 1@sw4-eth1 -i 2@sw4-eth2 -i 3@sw4-eth3 -i 4@sw4-eth4 -i 64@veth250 --thrift-port 10001 --pcap $*  /usr/share/bmpd/switch/switch.json
