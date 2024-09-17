#!/bin/bash

#
# free-port.sh - returns an unused TCP port.
#
PORT_START=4500
PORT_MAX=65000
port=$PORT_START

while true; do
  portsinuse=$(netstat --numeric-ports --numeric-hosts -a --protocol=tcpip | grep tcp |
    cut -c21- | cut -d':' -f2 | cut -d' ' -f1 | grep -E "[0-9]+" | uniq | tr "\n" " ")

  if echo "$portsinuse" | grep -wq "$port"; then
    if [ $port -eq $PORT_MAX ]; then
      exit 1
    fi
    port=$(("$port" + 1))
  else
    echo $port
    exit 0
  fi
done
