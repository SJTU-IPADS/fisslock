import socket
import sys

slaves = [
  ("192.168.12.47",  9201), # pro0
  ("192.168.12.138", 9202), # pro1
  ("192.168.12.9",  9203), # pro2
  ("192.168.12.97",  9204), # pro3
  ("192.168.12.47",  9205), # pro0
  ("192.168.12.138", 9206), # pro1
  ("192.168.12.9",  9207), # pro2
  ("192.168.12.97",  9208), # pro3
]

sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

if len(sys.argv) < 2:
  print("usage: ./send-signal [start|stop]")
  exit(0)

for s in slaves:
  sock.sendto(str.encode(sys.argv[1]), s)

