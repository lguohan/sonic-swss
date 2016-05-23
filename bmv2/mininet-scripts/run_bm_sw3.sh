sudo ./simple_switch --log-console  -i 1@sw3-eth1 -i 2@sw3-eth2 -i 3@sw3-eth3 -i 4@sw3-eth4 -i 64@veth250 --thrift-port 10001 --pcap $*  ../share/bmpd/switch/switch.json
