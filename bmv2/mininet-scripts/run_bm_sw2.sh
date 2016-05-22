sudo ./simple_switch --log-console  -i 1@sw2-eth1 -i 2@sw2-eth2 -i 3@sw2-eth3 -i 4@sw2-eth4 -i 64@veth250 --thrift-port 10001 --pcap $*  ../share/bmpd/switch/switch.json
