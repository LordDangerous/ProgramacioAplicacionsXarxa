import socket
import threading
from random import *

from select import *
from struct import *

HOST = 'localhost'  # Standard loopback interface address (localhost)
z = 2
t = 1


class Server:
    def __init__(self, id_server, udp_port, tcp_port):
        self.id_server = id_server
        self.udp_port = udp_port
        self.tcp_port = tcp_port


class Client:
    def __init__(self, id_client_client, state=None, ip=None):
        self.id_client_client = id_client_client
        self.state = state
        self.ip = ip


def read_file():
    id_server = udp_port = tcp_port = None
    f = open("server.cfg", "r")
    read = f.read()
    for line in read.splitlines():
        if 'Id =' in line:
            id_server = line.split('= ', 1)[1]
        elif 'UDP-port =' in line:
            udp_port = line.split('= ')[1]
        elif 'TCP-port =' in line:
            tcp_port = line.split('= ')[1]
    return Server(id_server, udp_port, tcp_port)


def read_database():
    clients = {}
    f = open("bbdd_dev.dat", "r")
    read = f.read()
    for line in read.splitlines():
        clients[line] = "DISCONNECTED"

    print(clients)
    return clients


def handle_udp_packet(sock, clients, server):
    package_type, id_client_transmitter, id_client_communication, data, address = read_udp(sock)
    register(package_type, id_client_transmitter, id_client_communication, data, address, sock, clients, server)


def read_udp(sock_udp):
    response = sock_udp.recvfrom(1024)
    data = response[0]
    address = response[1]
    print(f"Connected by {address} with data {data}")
    package_type, id_client_transmitter, id_client_communication, data = unpack_pdu(data)
    return package_type, id_client_transmitter, id_client_communication, data, address

    # sock_udp.sendto(data, address)


def read_tcp(sock_tcp):
    conn, address = sock_tcp.accept()
    print(f"Connected by {address}")
    data_tcp = conn.recv(1024)
    print(data_tcp)
    package_type, id_client_transmitter, id_client_communication, data = unpack_pdu(data_tcp)
    return package_type, id_client_transmitter, id_client_communication, data, conn

    # conn.sendall(data_tcp)


def pack_pdu(package_type, id_client_transmitter, id_client_communication, data):
    return pack("1s11s11s61s", bytes(package_type, "UTF-8"), bytes(id_client_transmitter, "UTF-8"),
                bytes(id_client_communication, "UTF-8"), bytes(data, "UTF-8"))


def unpack_pdu(pdu):
    package_type, id_client_transmitter, id_client_communication, data = unpack('1s11s11s61s', pdu)
    decoded_package_type = package_type.hex().rstrip('\x00')
    decoded_id_client_transmitter = id_client_transmitter.decode("UTF-8").rstrip('\x00')
    decoded_id_client_communication = id_client_communication.decode("UTF-8").rstrip('\x00')

    decoded_data = ""
    for byte in data:
        if byte == 0:
            break
        else:
            decoded_data += chr(byte)
            print(f"B: {byte}")
    # decoded_data = data.decode("UTF-8").split('\x00', 1)[0]
    print(f"Package type: {decoded_package_type} length: {len(decoded_package_type)}")
    print(f"id_client trasmitter: {decoded_id_client_transmitter} length: {len(decoded_id_client_transmitter)}")
    print(f"id_client Communication: {decoded_id_client_communication}")
    print(f"Data: {decoded_data}")
    return decoded_package_type, decoded_id_client_transmitter, decoded_id_client_communication, decoded_data


def register(package_type, id_client_transmitter, id_client_communication, data, address, sock, clients, server):
    print(f"id_client transmitter: {id_client_transmitter}; id_client communication: {id_client_communication}; data: {data}")
    if id_client_transmitter in clients and id_client_communication == "0000000000" and data == "" and clients[id_client_transmitter] == "DISCONNECTED":
        random_number = str(randint(1000000000, 9999999999))

        # Open new UDP port
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        newport = int(server.udp_port) + 1
        sock.bind((HOST, newport))

        sock.sendto(pack_pdu('0xa1', server.id_client, random_number, str(newport)), address)
        clients[id_client_transmitter] = "WAIT_INFO"

        input_sock = [sock]
        for i in range(z):
            input_ready, output_ready, except_ready = select(input_sock, [], [], t)

            if input_ready:
                package_type, id_client_transmitter, id_client_communication, data, address = read_udp(sock, clients)
                # Fer canvi client a classe i comprovació dades
                if id_client_transmitter in clients and id_client_communication == server.id_client:
                    print("OK")
                    return

        clients[id_client_transmitter] = "DISCONNECTED"
        print(f"Client {id_client_transmitter} desconnectat perquè s'ha exhaurit el temps {z}")
        # TANCAR SOCKET ???????
        sock.close()




        # TODO

    else:
        print(pack_pdu('0xa3', server.id_server, "0000000000", "Rebuig de registre"))
        sock.sendto(pack_pdu('0xa3', server.id_server, "0000000000", "Rebuig de registre"), address)
        clients[id_client_transmitter] = "DISCONNECTED"
        print(f"Client {id_client_transmitter} desconnectat per rebuig de registre: {clients}")


def setup():
    server = read_file()
    clients = read_database()

    # UDP Wait packet
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp.bind((HOST, int(server.udp_port)))

    # TCP Wait packet
    sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_tcp.bind((HOST, int(server.tcp_port)))
    sock_tcp.listen()

    input = [sock_udp, sock_tcp]

    while True:
        input_ready, output_ready, except_ready = select(input, [], [])

        for sock in input_ready:
            if sock == sock_udp:
                thread = threading.Thread(target=handle_udp_packet, args=(sock, clients, server))
                thread.start()
                # handle_udp_packet(sock, clients, server)
                # package_type, id_client_transmitter, id_client_communication, data, address = read_udp(sock, clients)
                # register(package_type, id_client_transmitter, id_client_communication, data, address, sock, clients, server)
            elif sock == sock_tcp:
                return
                # Replica UDP no usable
                # package_type, id_client_transmitter, id_client_communication, data, conn = read_tcp(sock)
                # register(package_type, id_client_transmitter, id_client_communication, data, conn, clients, server)
            else:
                print(f"unknown socket: {sock}")


if __name__ == '__main__':
    setup()
