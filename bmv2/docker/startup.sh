#!/bin/bash

echo "Veth setup"
cd /sonic-swss/bmv2/switch/tools
sudo ./veth_setup.sh  > /tmp/veth_setup.log 2>&1
sleep 10

echo "Disable IPv6"
sudo ./veth_disable_ipv6.sh > /tmp/veth_disable.log 2>&1
sleep 10

echo "Start rsyslog"
sudo rsyslogd &

echo "Start BMV2"
cd ../../install/bin
../../run_bm.sh > /tmp/run_bm.log 2>&1 &
sleep 20

echo "Start redis server"
redis-server > /tmp/redis-server.log 2>&1 &
sleep 10

echo "Start Syncd"
cd /sonic-sairedis/syncd
sudo LD_LIBRARY_PATH=/sonic-swss/bmv2/install/lib:$LD_LIBRARY_PATH ./syncd > /tmp/syncd.log 2>&1  &
sleep 10

echo "Start Orchagent"
cd /sonic-swss/orchagent
sudo LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH ./orchagent > /tmp/orchagent.log 2>&1 &
sleep 10

echo "Start Portsyncd"
cd ../portsyncd
sudo ./portsyncd -f port_config.ini > /tmp/portsyncd.log 2>&1 &
sleep 5

echo "Start Intfsync"
cd ../intfsyncd
sudo ./intfsyncd > /tmp/intfsyncd.log 2>&1 &
sleep 5

echo "Start Neighsyncd"
cd ../neighsyncd
sudo ./neighsyncd > /tmp/neighsyncd.log 2>&1 &
sleep 10

#echo "Start Routeresync"
#sudo ./routeresync start > /tmp/routeresync.log 2>&1 &
#sleep 10
#
#echo "Start Fpmsyncd"
#cd ../fpmsyncd
#sudo ./fpmsyncd > /tmp/fpmsyncd.log 2>&1 &
#sleep 5

cd /
