import socket
import threading
from random import *

from select import *
from struct import *

HOST = 'localhost'  # Standard loopback interface address (localhost)
z = 2
t = 1


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
    clients = {}
    f = open("bbdd_dev.dat", "r")
    read = f.read()
    for line in read.splitlines():
        clients[line] = "DISCONNECTED"

    print(clients)
    return clients


def handleUDPpacket(sock, clients, server):
    packagetype, idtransmitter, idcommunication, data, address = read_udp(sock)
    register(packagetype, idtransmitter, idcommunication, data, address, sock, clients, server)


def read_udp(sockUDP):
    response = sockUDP.recvfrom(1024)
    data = response[0]
    address = response[1]
    print(f"Connected by {address} with data {data}")
    packagetype, idtransmitter, idcommunication, data = unpackPDU(data)
    return packagetype, idtransmitter, idcommunication, data, address

    #sockUDP.sendto(data, address)


def read_tcp(sockTCP, clients):
    conn, address = sockTCP.accept()
    print(f"Connected by {address}")
    dataTCP = conn.recv(1024)
    print(dataTCP)
    packagetype, idtransmitter, idcommunication, data = unpackPDU(dataTCP)
    return packagetype, idtransmitter, idcommunication, data, conn

    # conn.sendall(dataTCP)


def packPDU(packagetype, idtransmitter, idcommunication, data):
    return pack("1s11s11s61s", bytes(packagetype, "UTF-8"), bytes(idtransmitter, "UTF-8"),
                bytes(idcommunication, "UTF-8"), bytes(data, "UTF-8"))


def unpackPDU(PDU):
    packagetype, idtransmitter, idcommunication, data = unpack('1s11s11s61s', PDU)
    decodedpackagetype = packagetype.hex().rstrip('\x00')
    decodedidtransmitter = idtransmitter.decode("UTF-8").rstrip('\x00')
    decodedidcommunication = idcommunication.decode("UTF-8").rstrip('\x00')
    decodeddata = data.decode("UTF-8").split('\x00', 1)[0]
    print(f"Package type: {decodedpackagetype} length: {len(decodedpackagetype)}")
    print(f"ID Trasmitter: {decodedidtransmitter} length: {len(decodedidtransmitter)}")
    print(f"ID Communication: {decodedidcommunication}")
    print(f"Data: {decodeddata}")
    return decodedpackagetype, decodedidtransmitter, decodedidcommunication, decodeddata


def register(packagetype, idtransmitter, idcommunication, data, address, sock, clients, server):
    if idtransmitter in clients and idcommunication == "0000000000" and data == "" and clients[idtransmitter] == "DISCONNECTED":
        randomnumber = str(randint(1000000000, 9999999999))

        #Open new UDP port
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        newport = int(server.UDPport) + 1
        sock.bind((HOST, newport))

        sock.sendto(packPDU('0xa1', server.id, randomnumber, str(newport)), address)
        clients[idtransmitter] = "WAIT_INFO"

        inputsock = [sock]
        for i in range(z):
            inputready, outputready, exceptready = select(inputsock, [], [], t)

            if inputready:
                packagetype, idtransmitter, idcommunication, data, address = read_udp(sock, clients)
                ##Fer canvi client a classe i comprovació dades
                if idtransmitter in clients and idcommunication == server.id:
                    print("OK")
                    return

        clients[idtransmitter] = "DISCONNECTED"
        print(f"Client {idtransmitter} desconnectat perquè s'ha exhaurit el temps {z}")
        # TANCAR SOCKET ???????
        sock.close()




        # TODO

    else:
        print(packPDU('0xa3', server.id, "0000000000", "Rebuig de registre"))
        sock.sendto(packPDU('0xa3', server.id, "0000000000", "Rebuig de registre"), address)
        clients[idtransmitter] = "DISCONNECTED"
        print(f"Client {idtransmitter} desconnectat per rebuig de registre: {clients}")


def setup():
    server = Server
    read_file(server)

    clients = read_database()

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
                thread = threading.Thread(target=handleUDPpacket, args=(sock, clients, server))
                thread.start()
                # handleUDPpacket(sock, clients, server)
                # packagetype, idtransmitter, idcommunication, data, address = read_udp(sock, clients)
                # register(packagetype, idtransmitter, idcommunication, data, address, sock, clients, server)
            elif sock == sockTCP:
                return
                #Replica UDP no usable
                #packagetype, idtransmitter, idcommunication, data, conn = read_tcp(sock, clients)
                #register(packagetype, idtransmitter, idcommunication, data, conn, clients, server)
            else:
                print(f"unknown socket: {sock}")


if __name__ == '__main__':
    setup()
