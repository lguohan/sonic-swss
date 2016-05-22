sudo ./simple_switch --log-console  -i 0@veth0 -i 1@veth2 -i 2@veth4 -i 3@veth6 -i 4@veth8 -i 5@veth10 -i 6@veth12 -i 7@veth14 -i 8@veth16 -i 64@veth250 --thrift-port 10001 --pcap $*  ../share/bmpd/switch/switch.json

#sudo ./simple_switch --log-console  -i 1@sw1-eth1 -i 2@sw1-eth2 -i 3@sw1-eth3 -i 4@sw1-eth4 -i 64@veth250 --thrift-port 10001 --pcap $*  ../share/bmpd/switch/switch.json

