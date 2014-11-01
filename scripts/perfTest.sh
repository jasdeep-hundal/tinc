#!/bin/bash

NETWORK_NAME=jasdeepLocal

REMOTE_HOST_IP=192.168.56.102
REMOTE_HOST_USER=jasdeep
REMOTE_HOST_PASSWORD=golfing

REMOTE_HOST_TINC_IP=192.168.57.2

function remote_host_command {
(
    /usr/bin/expect << EOD
    spawn ssh -t $REMOTE_HOST_USER@$REMOTE_HOST_IP "$1"
    expect -nocase "password:"
    send "$REMOTE_HOST_PASSWORD\r"
    expect eof
EOD
)
}

# Start tincd on this host and get its process id
sudo tincd -n $NETWORK_NAME
LOCAL_TINCD_PID=`pgrep tincd`
# Start tincd on the remote host and get its process id
remote_host_command "sudo tincd -n $NETWORK_NAME" > /dev/null
# Start netperf server on remote host and get its process id
remote_host_command "nohup netserver" > /dev/null

echo "Testing bulk data performance w/o tinc VPN"
netperf -H $REMOTE_HOST_IP

echo "Testing bulk data performance w/ tinc VPN"
netperf -H $REMOTE_HOST_TINC_IP

# Shut down netserver on remote host
remote_host_command "sudo pkill -3 netserver" > /dev/null
# Shut down tincd on remote host
remote_host_command "sudo pkill -3 tincd" > /dev/null
# Shut down tincd on this host
sudo pkill -3 tincd
