import socket
from struct import *


class Client:
    def __init__(self, id_client, elements, local_tcp, server, server_udp):
        self.id_client = id_client
        self.elements = elements
        self.local_tcp = local_tcp
        self.server = server
        self.server_udp = int(server_udp)


def pack_pdu(package_type, id_client_transmitter, id_client_communication, data):
    return pack("1s11s11s61s", bytes.fromhex('a0'), bytes(id_client_transmitter, "UTF-8"), bytes(id_client_communication, "UTF-8"), bytes(data, "UTF-8"))


class PDU:
    def __init__(self, package_type, id_client_transmitter, id_client_communication, data=None):
        self.package_type = package_type,
        self.id_client_transmitter = id_client_transmitter,
        self.id_client_communication = id_client_communication,
        self.data = data


def read_file():
    id_client = elements = local_tcp = server = server_udp = None
    f = open("client.cfg", "r")
    read = f.read()
    for line in read.splitlines():
        if 'Id =' in line:
            id_client = line.split('= ', 1)[1]
        elif 'Elements =' in line:
            elements = line.split('= ')[1]
        elif 'Local-TCP =' in line:
            local_tcp = line.split('= ')[1]
        elif 'Server =' in line:
            server = line.split('= ')[1]
        elif 'Server-UDP =' in line:
            server_udp = line.split('= ')[1]
    return Client(id_client, elements, local_tcp, server, server_udp)


def send_udp_packet(client):
    udp_connexion = (client.server, client.server_udp)
    pdu_REG_REQ = pack_pdu('0xa0', client.id_client, "0000000000", "aaaa01\x001")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(pdu_REG_REQ, udp_connexion)
    print(pdu_REG_REQ)
    data = sock.recv(1024).decode("UTF-8")
    sock.close()

    print(data)


def send_tcp_packet(client):
    tcp_connexion = (client.server, 2202)
    pdu_REG_REQ = pack_pdu('0xa0', client.id_client_client, "0000000000", "")
    print(pdu_REG_REQ)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(tcp_connexion)
    sock.send(pdu_REG_REQ)
    data = sock.recv(1024)
    print(data)


def setup():
    client = read_file()
    send_udp_packet(client)


if __name__ == '__main__':
    setup()




