INSTALLATION
============
Installation steps (performed in the base directory of the tinc source):

    autoreconf -i
    ./configure --sysconfdir=/etc --localstatedir=/var
    sudo make install

Make sure the following packages are installed:
libncurses5-dev libreadline-dev zlibc liblzo2-dev libssl-dev autoconf libtool texinfo

CONFIGURATION
=============
These settings are based on CentOS 6.5 VMs running on VirtualBox, so you mileage
may vary. If you are running tinc on virtual machines, it is highly recommended
that you enable paravirtualized networking if you have support for it.

This configuration assumes that the VMs are connected via a host only network
with adapters that are assigned the IP address 192.168.57.1 for VM1 and
192.168.57.2 for VM2. In VirtualBox, this can be setup in Preferences -> Network
by adding a network with a host only adapter with IPv4 address 192.168.56.1 and
a network mask of 255.255.255.0. Then the DHCP Server for the network can be
configured with the server address 192.168.56.100, the server mask 255.255.255.0,
and address bounds between 192.168.56.101 and 192.168.56.254.


Refer to the tinc VPN installation/configuration docs for more information on
these values.

The first step for the configuration is to set up each host with each of the
listed configuration files EXCEPT for the hosts files that refer to different
hosts. Then generate the public/private keypair for the host using:
    sudo tincd -K YOUR_NETWORK_NAME
The private key should be saved in the file rsa_key.priv in
/etc/tinc/YOUR_NETWORK_NAME/ and the public key should be appended to the
appropriate host file in /etc/tinc/YOUR_NETWORK_NAME/hosts/

VM1
---
/etc/tinc/YOUR_NETWORK_NAME/tinc.conf:

    Name = vm1
    Device=/dev/net/tun
    ConnectTo = vm2
    ProcessPriority = high

/etc/tinc/YOUR_NETWORK_NAME/tinc-up:

    #!/bin/sh
    ifconfig $INTERFACE 192.168.57.1 netmask 255.255.255.0

/etc/tinc/YOUR_NETWORK_NAME/tinc-down:

    #!/bin/sh
    ifconfig $INTERFACE down

/etc/tinc/YOUR_NETWORK_NAME/hosts/vm1:

    Address = 192.168.56.101
    Subnet = 192.168.57.1/32

    Cipher = none
    Digest = none

/etc/tinc/YOUR_NETWORK_NAME/hosts/vm2:

    COPY THIS FILE FROM VM2

VM2
---
/etc/tinc/YOUR_NETWORK_NAME/tinc.conf:

    Name = vm2
    Device=/dev/net/tun
    ConnectTo = vm1
    ProcessPriority = high

/etc/tinc/YOUR_NETWORK_NAME/tinc-up:

    #!/bin/sh
    ifconfig $INTERFACE 192.168.57.2 netmask 255.255.255.0

/etc/tinc/YOUR_NETWORK_NAME/tinc-down:

    #!/bin/sh
    ifconfig $INTERFACE down

/etc/tinc/YOUR_NETWORK_NAME/hosts/vm1:

    COPY THIS FILE FROM VM1

/etc/tinc/YOUR_NETWORK_NAME/hosts/vm2:

    Address = 192.168.56.102
    Subnet = 192.168.57.2/32

    Cipher = none
    Digest = none

BASIC USAGE
===========
To start tinc:

    sudo tincd -n YOUR_NETWORK_NAME

To stop tinc:

    sudo kill -3 TINCD_PROCESS_ID

You can also use the following to start tinc in the foreground for debugging:

    sudo tincd -n YOUR_NETWORK_NAME -D -d3

REQUIREMENTS FOR TEST SCRIPTS
=============================
Install netperf and run netserver as a daemon (helpful to do on both VMs):

    sudo yum install netperf

The users used for tinc testing on both hosts must have passwordless sudo access.

TEST SCRIPTS
============
Start tincd and then netserver on one VM and the run of the following scripts on
the other VM.

Compare network performance w/ and w/o tinc:

    ./perfTest.sh

