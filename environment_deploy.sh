ip netns d vhostsw
ip netns d sniffsw
ip netns d cli1
ip netns d cli2
sysctl -w net.ipv4.ip_forward=0 > /dev/null
firewall-cmd --reload > /dev/null
ip l d glob > /dev/null 2>&1

ip netns a vhostsw
ip netns a sniffsw
ip netns a cli1
ip netns a cli2
ip netns e vhostsw ip l s lo up
ip netns e sniffsw ip l s lo up
ip netns e cli1 ip l s lo up
ip netns e cli2 ip l s lo up


ip l a eth0 type veth peer name glob
ip l s eth0 netns sniffsw
ip netns e sniffsw ip l s eth0 up

ip a a 10.0.0.1/24 dev glob
ip l s glob up


ip l a eth0 type veth peer name eth1
ip l s eth0 netns cli1
ip netns e cli1 ip a a 10.0.0.11/24 dev eth0
ip netns e cli1 ip l s eth0 up
ip l s eth1 netns vhostsw
ip netns e vhostsw ip l s eth1 up


ip l a eth0 type veth peer name eth2
ip l s eth0 netns cli2
ip netns e cli2 ip a a 10.0.0.12/24 dev eth0
ip netns e cli2 ip l s eth0 up
ip l s eth2 netns vhostsw
ip netns e vhostsw ip l s eth2 up


ip l a eth0 type veth peer name eth1
ip l s eth0 netns vhostsw
ip netns e vhostsw ip l s eth0 up
ip l s eth1 netns sniffsw
ip netns e sniffsw ip l s eth1 up


ip netns e vhostsw brctl addbr br0 
ip netns e vhostsw brctl addif br0 eth0 eth1 eth2
ip netns e vhostsw ip l s br0 up

ip netns e sniffsw brctl addbr br0
ip netns e sniffsw brctl addif br0 eth0 eth1
ip netns e sniffsw ip l s br0 up


ip netns e cli1 ip r a 0/0 via 10.0.0.1 
ip netns e cli2 ip r a 0/0 via 10.0.0.1 

sysctl -w net.ipv4.ip_forward=1 > /dev/null
iptables -t nat -A POSTROUTING -o eno16777736 -j MASQUERADE
iptables -A FORWARD -i eno16777736 -o glob -j ACCEPT
iptables -A FORWARD -i glob -o eno16777736 -j ACCEPT

