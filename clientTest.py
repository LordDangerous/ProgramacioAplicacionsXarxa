import socket
from struct import *


class Client:
    def __init__(self, id, localTCP, server, serverUDP, elements, element1=None, element2=None, element3=None, element4=None, element5=None):
        self.id = id,
        self.elements = elements,
        self.element1 = element1,
        self.element2 = element2,
        self.element3 = element3,
        self.element4 = element4,
        self.element5 = element5,
        self.localTCP = localTCP,
        self.server = server,
        self.serverUDP = serverUDP

    def setElements(self, elements):
        for i in range(len(elements)):
            if i == 0 and elements[0]:
                self.element1 = elements[0]
            if i == 1 and elements[1]:
                self.element2 = elements[1]
            if i == 2 and elements[2]:
                self.element3 = elements[2]
            if i == 3 and elements[3]:
                self.element4 = elements[3]
            if i == 4 and elements[4]:
                self.element5 = elements[4]

def packPDU(packagetype, idtransmitter, idcommunication, data):
    return pack("1s11s11s61s", bytes.fromhex('a0'), bytes(idtransmitter, "UTF-8"), bytes(idcommunication, "UTF-8"), bytes(data, "UTF-8"))




class PDU:
    def __init__(self, packagetype, idtransmitter, idcommunication, data=None):
        self.packagetype = packagetype,
        self.idtransmitter = idtransmitter,
        self.idcommunication = idcommunication,
        self.data = data



def readfile(client):
    f = open("client.cfg", "r")
    read = f.read()
    for line in read.splitlines():
        if 'Id =' in line:
            client.id = line.split('= ', 1)[1]
        elif 'Elements =' in line:
            elements = line.split('= ')[1]
            client.elements = elements.split(';')
        elif 'Local-TCP =' in line:
            client.localTCP = line.split('= ')[1]
        elif 'Server =' in line:
            client.server = line.split('= ')[1]
        elif 'Server-UDP =' in line:
            client.serverUDP = line.split('= ')[1]

def sendUDPPacket(client):
    UDPConnexion = (client.server, int(client.serverUDP))
    pduREG_REQ = packPDU('0xa0', client.id, "0000000000", "")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(pduREG_REQ, UDPConnexion)
    print(pduREG_REQ)
    data = sock.recv(1024).decode("UTF-8")
    sock.close()

    print(data)

def sendTCPPacket(client):
    TCPConnexion = (client.server, 2202)
    pduREG_REQ = packPDU('0xa0', client.id, "0000000000", "")
    print(pduREG_REQ)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(TCPConnexion)
    sock.send(pduREG_REQ)
    data = sock.recv(1024)
    print(data)

def setup():
    client = Client
    readfile(client)
    client.setElements(client, client.elements)
    sendUDPPacket(client)





if __name__ == '__main__':
    setup()




