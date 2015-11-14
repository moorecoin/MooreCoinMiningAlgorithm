#!/usr/bin/env python
#
# generate seeds.txt from pieter's dns seeder
#

nseeds=512

max_seeds_per_asn=2

min_blocks = 337600

# these are hosts that have been observed to be behaving strangely (e.g.
# aggressively connecting to every node).
suspicious_hosts = set([
    "130.211.129.106", "178.63.107.226",
    "83.81.130.26", "88.198.17.7", "148.251.238.178", "176.9.46.6",
    "54.173.72.127", "54.174.10.182", "54.183.64.54", "54.194.231.211",
    "54.66.214.167", "54.66.220.137", "54.67.33.14", "54.77.251.214",
    "54.94.195.96", "54.94.200.247"
])

import re
import sys
import dns.resolver

pattern_ipv4 = re.compile(r"^((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})):8333$")
pattern_agent = re.compile(r"^(\/satoshi:0.8.6\/|\/satoshi:0.9.(2|3)\/|\/satoshi:0.10.\d{1,2}\/)$")

def parseline(line):
    sline = line.split()
    if len(sline) < 11:
       return none
    # match only ipv4
    m = pattern_ipv4.match(sline[0])
    if m is none:
        return none
    # do ipv4 sanity check
    ip = 0
    for i in range(0,4):
        if int(m.group(i+2)) < 0 or int(m.group(i+2)) > 255:
            return none
        ip = ip + (int(m.group(i+2)) << (8*(3-i)))
    if ip == 0:
        return none
    # skip bad results.
    if sline[1] == 0:
        return none
    # extract uptime %.
    uptime30 = float(sline[7][:-1])
    # extract unix timestamp of last success.
    lastsuccess = int(sline[2])
    # extract protocol version.
    version = int(sline[10])
    # extract user agent.
    agent = sline[11][1:-1]
    # extract service flags.
    service = int(sline[9], 16)
    # extract blocks.
    blocks = int(sline[8])
    # construct result.
    return {
        'ip': m.group(1),
        'ipnum': ip,
        'uptime': uptime30,
        'lastsuccess': lastsuccess,
        'version': version,
        'agent': agent,
        'service': service,
        'blocks': blocks,
    }

# based on greg maxwell's seed_filter.py
def filterbyasn(ips, max_per_asn, max_total):
    result = []
    asn_count = {}
    for ip in ips:
        if len(result) == max_total:
            break
        try:
            asn = int([x.to_text() for x in dns.resolver.query('.'.join(reversed(ip['ip'].split('.'))) + '.origin.asn.cymru.com', 'txt').response.answer][0].split('\"')[1].split(' ')[0])
            if asn not in asn_count:
                asn_count[asn] = 0
            if asn_count[asn] == max_per_asn:
                continue
            asn_count[asn] += 1
            result.append(ip)
        except:
            sys.stderr.write('err: could not resolve asn for "' + ip['ip'] + '"\n')
    return result

def main():
    lines = sys.stdin.readlines()
    ips = [parseline(line) for line in lines]

    # skip entries with valid ipv4 address.
    ips = [ip for ip in ips if ip is not none]
    # skip entries from suspicious hosts.
    ips = [ip for ip in ips if ip['ip'] not in suspicious_hosts]
    # enforce minimal number of blocks.
    ips = [ip for ip in ips if ip['blocks'] >= min_blocks]
    # require service bit 1.
    ips = [ip for ip in ips if (ip['service'] & 1) == 1]
    # require at least 50% 30-day uptime.
    ips = [ip for ip in ips if ip['uptime'] > 50]
    # require a known and recent user agent.
    ips = [ip for ip in ips if pattern_agent.match(ip['agent'])]
    # sort by availability (and use last success as tie breaker)
    ips.sort(key=lambda x: (x['uptime'], x['lastsuccess'], x['ip']), reverse=true)
    # look up asns and limit results, both per asn and globally.
    ips = filterbyasn(ips, max_seeds_per_asn, nseeds)
    # sort the results by ip address (for deterministic output).
    ips.sort(key=lambda x: (x['ipnum']))

    for ip in ips:
        print ip['ip']

if __name__ == '__main__':
    main()
