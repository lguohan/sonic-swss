#!/usr/bin/env python

from swsscommon import swsscommon
from datetime import datetime
import ipaddress
import random
import time
import sys
import re
import subprocess
import argparse

delete_ratio = 0
replace_ratio = 0

def create_route(p, neigh, neigh6, psr):

    pfx = ipaddress.ip_network(p)
    if pfx.version == 4:
        gsize = random.randint(1,4)
        random.shuffle(neigh)
        route = (p, neigh[0:gsize])
    else:
        gsize = random.randint(1,4)
        random.shuffle(neigh6)
        route = (p, neigh6[0:gsize])

    nhs = ",".join([ x[1] for x in route[1] ])
    ifs = ",".join([ x[0] for x in route[1] ])
    fvs = swsscommon.FieldValuePairs([('nexthop', nhs), ('ifname', ifs)])
    psr.set(route[0], fvs)

def remove_route(p, psr):
    psr._del(p)

def update_routes(prefixes, neigh, neigh6, psr):
        
    added_prefix = []
    for pfx in prefixes:
        create_route(pfx, neigh, neigh6, psr)
        index = random.randint(0, len(added_prefix))
        added_prefix.insert(index, pfx)
        p = random.random()
        if p < delete_ratio and len(added_prefix) > 0:
            del_prefix = added_prefix.pop()
            remove_route(del_prefix, psr)
        p = random.random()
        if p < replace_ratio and len(added_prefix) > 0:
            index = random.randint(0, len(added_prefix)-1)
            create_route(added_prefix[index], neigh, neigh6, psr)

    for pfx in added_prefix:
        remove_route(pfx, psr)

def create_neigh(neigh, psn):

    for nei in neigh:
        ip = ipaddress.ip_address(nei[1])
        if ip.version == 4:
            fvs = swsscommon.FieldValuePairs([('neigh', '00:11:11:11:11:11'), ('family', 'IPv4')])
        else:
            fvs = swsscommon.FieldValuePairs([('neigh', '00:11:11:11:11:11'), ('family', 'IPv6')])

        psn.set("%s:%s" % (nei[0], nei[1]), fvs)
        

def find_neighbors(db):

    neigh = []
    neigh6 = []
    intf_tbl = swsscommon.Table(db, "INTF_TABLE")
    for key in intf_tbl.getKeys():
        (port, pfx) = key.split(':', 1)
        if not port.startswith("Ethernet") and not port.startswith("PortChannel"):
            continue
        ipnet = ipaddress.ip_interface(pfx).network
        ip = pfx.split('/')[0]
        for ipnei in ipnet.hosts():
            if ip != str(ipnei):
                if ipnet.version == 4:
                    neigh.append((port, str(ipnei)))
                else:
                    neigh6.append((port, str(ipnei)))

    return (neigh, neigh6)

def check_process(procname, dockername=None):
    if dockername:
        s = subprocess.Popen(["/usr/bin/docker", "exec", "-it", dockername, "ps", "ax"], stdout=subprocess.PIPE)
    else:
        s = subprocess.Popen(["ps", "ax"], stdout=subprocess.PIPE)

    for x in s.stdout:
        if re.search(procname, x):
            return True
    return False
 
def main():
    global delete_ratio
    global replace_ratio

    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('--routes', type=int, default=6400, help='Number of routes')
    parser.add_argument('--round', type=int, default=10, help='Number of rounds')
    parser.add_argument('--sleep', type=int, default=5, help='Quiet period between rounds in seconds')
    parser.add_argument('--type', action='store', default="both", choices=["v4", "v6", "both"], help='Test IPv4 only')
    parser.add_argument('--delete', type=int, default=0, help='Route deletion ratio (0 to 100)')
    parser.add_argument('--replace', type=int, default=0, help='Route replacement ratio (0 to 100)')
    parser.add_argument('--nhgsize', type=int, default=4, help='Maximum next hop group size')
    parser.add_argument('--docker', action='store', help='Syncd container name')
    args = parser.parse_args()

    delete_ratio = args.delete/100.0
    replace_ratio = args.replace/100.0

    db = swsscommon.DBConnector(0, "/var/run/redis/redis.sock", 0)

    (neigh, neigh6) = find_neighbors(db) 
    if args.type == "both" or args.type == "v4":
        if len(neigh) < args.nhgsize:
            print "Not enough IPv4 neighors %d" % len(neigh)
            sys.exit(1)
    if args.type == "both" or args.type == "v6":
        if len(neigh6) < args.nhgsize:
            print "Not enough IPv6 neighors %d" % len(neigh6)
            sys.exit(1)

    psn = swsscommon.ProducerStateTable(db, "NEIGH_TABLE")
    create_neigh(neigh[0:args.nhgsize], psn)
    create_neigh(neigh6[0:args.nhgsize], psn)

    psr = swsscommon.ProducerStateTable(db, "ROUTE_TABLE")

    iplist = []
    if args.type == "both" or args.type == "v4": 
        ip0 = int(ipaddress.IPv4Address("173.0.0.0"))
        for i in range(args.routes):
            prefix = "%s/26" % ipaddress.IPv4Address(ip0 + 256 * i)
            iplist.append(prefix)

    if args.type == "both" or args.type == "v6":
        ip0 = int(ipaddress.IPv6Address("20c0:bbf4:0:40::"))>>64
        for i in range(args.routes):
            prefix = "%s/64" % ipaddress.IPv6Address(((((ip0>>32)+i)<<32)+(ip0&0xffffffff))<<64)
            iplist.append(prefix)

    if check_process("/usr/bin/syncd", args.docker) == False:
        print "syncd is not running"
        sys.exit(1)

    starttime = datetime.now()
    lasttime = datetime.now()

    syncd_crashed = False
    while True:
        print "test %d routes... %s" % (args.routes, datetime.now() - lasttime)
        lastime = datetime.now()
			
        random.shuffle(iplist)
        update_routes(iplist, neigh, neigh6, psr)
        syncd_crashed = check_process("/usr/bin/syncd", args.docker)
        if syncd_crashed == False:
            print "syncd crashed in %s!" % (datetime.now() - starttime)
            break
        time.sleep(args.sleep)

    if syncd_crashed == False:
        duration = datetime.now() - starttime
        print "Good job! syncd sustained for %s seconds" % duration

if __name__ == "__main__":
    main()
