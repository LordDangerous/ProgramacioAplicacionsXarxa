import socket
import time
from select import select

HOST = 'localhost'  # Standard loopback interface address (localhost)


class Server:
    def __init__(self, id, UDPport, TCPport):
        self.id = id,
        self.UDPport = UDPport,
        self.TCPport = TCPport


class Client:
    def __init__(self, id, state=None, IP=None):
        self.id = id,
        self.state = state,
        self.IP = IP


def read_file(server):
    f = open("server.cfg", "r")
    read = f.read()
    for line in read.splitlines():
        if 'Id =' in line:
            server.id = line.split('= ', 1)[1]
        elif 'UDP-port =' in line:
            server.UDPport = line.split('= ')[1]
        elif 'TCP-port =' in line:
            server.TCPport = line.split('= ')[1]


def read_database():
    clients = []
    f = open("bbdd_dev.dat", "r")
    read = f.read()
    for line in read.splitlines():
        clients.append(line)

    return clients


def read_udp(sockUDP):
    response = sockUDP.recvfrom(1024)
    data = response[0]
    address = response[1]
    print(f"Connected by {address} with data {data}")
    sockUDP.sendto(data, address)



def read_tcp(sockTCP):
    conn, address = sockTCP.accept()
    print(f"Connected by {address}")
    dataTCP = conn.recv(1024)
    conn.sendall(dataTCP)


def setup():
    server = Server
    read_file(server)
    clients = read_database()
    print(clients)

    # UDP Wait packet
    sockUDP = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sockUDP.bind((HOST, int(server.UDPport)))

    # TCP Wait packet
    sockTCP = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockTCP.bind((HOST, int(server.TCPport)))
    sockTCP.listen()

    input = [sockUDP, sockTCP]

    while True:
        inputready, outputready, exceptready = select(input, [], [])

        for sock in inputready:
            if sock == sockUDP:
                read_udp(sock)
            elif sock == sockTCP:
                read_tcp(sock)
            else:
                print(f"unknown socket: {sock}")



if __name__ == '__main__':
    setup()
