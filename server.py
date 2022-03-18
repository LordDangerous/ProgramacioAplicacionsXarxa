import socket
import sys
import threading
from random import *
import logging

from select import *
from struct import *

HOST = 'localhost'  # Standard loopback interface address (localhost)
z = 2
t = 1

# Logging config
logging.basicConfig(format='%(asctime)s - %(levelname)s => %(message)s', datefmt='%H:%M:%S', level=logging.INFO)
logging.basicConfig(format='%(asctime)s - %(levelname)s => %(message)s', datefmt='%H:%M:%S', level=logging.DEBUG)


class Server:
    def __init__(self, id_server, udp_port, tcp_port):
        self.id_server = id_server
        self.udp_port = int(udp_port)
        self.tcp_port = tcp_port


class Client:
    def __init__(self, id_client, state=None, tcp_port=None, elements=None):
        self.id_client = id_client
        self.state = state
        self.tcp_port = tcp_port
        self.elements = elements


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


# def read_database():
#     clients = {}
#     f = open("bbdd_dev.dat", "r")
#     read = f.read()
#     for line in read.splitlines():
#         clients[line] = "DISCONNECTED"
#
#     logging.info(clients)
#     return clients

def read_database():
    f = open("bbdd_dev.dat", "r")
    read = f.read()
    clients = []
    for line in read.splitlines():
        clients.append(Client(line, "DISCONNECTED"))

    client_info = "\nCLIENT ID\tCLIENT STATE\n"
    for client in clients:
        client_info += f"{client.id_client}\t{client.state}\n"
    logging.info(client_info)
    return clients


def handle_udp_packet(sock, clients, server):
    package_type, id_client_transmitter, id_client_communication, data, address = read_udp(sock)
    register(package_type, id_client_transmitter, id_client_communication, data, address, sock, clients, server)


def read_udp(sock_udp):
    response = sock_udp.recvfrom(1024)
    data = response[0]
    address = response[1]
    logging.info(f"Connected by {address} with data {data}")
    package_type, id_client_transmitter, id_client_communication, data = unpack_pdu(data)
    return package_type, id_client_transmitter, id_client_communication, data, address

    # sock_udp.sendto(data, address)


def read_tcp(sock_tcp):
    conn, address = sock_tcp.accept()
    logging.info(f"Connected by {address}")
    data_tcp = conn.recv(1024)
    logging.info(data_tcp)
    package_type, id_client_transmitter, id_client_communication, data = unpack_pdu(data_tcp)
    return package_type, id_client_transmitter, id_client_communication, data, conn

    # conn.sendall(data_tcp)


def pack_pdu(package_type, id_client_transmitter, id_client_communication, data):
    return pack("1s11s11s61s", bytes.fromhex(package_type), bytes(id_client_transmitter, "UTF-8"),
                bytes(str(id_client_communication), "UTF-8"), bytes(str(data), "UTF-8"))


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
            logging.info(f"B: {byte}")
    # decoded_data = data.decode("UTF-8").split('\x00', 1)[0]
    logging.info("-------------------------------- UNPACK PDU -----------------------------")
    logging.info(f"Package type: {decoded_package_type} length: {len(decoded_package_type)}")
    logging.info(f"id_client trasmitter: {decoded_id_client_transmitter} length: {len(decoded_id_client_transmitter)}")
    logging.info(
        f"id_client Communication: {decoded_id_client_communication}; length: {len(decoded_id_client_communication)}")
    logging.info(f"Data: {decoded_data}; length: {len(decoded_data)}")
    logging.info("------------------------------ END UNPACK PDU ---------------------------\n")
    return decoded_package_type, decoded_id_client_transmitter, decoded_id_client_communication, decoded_data


def check_client(id_client_transmitter, clients):
    for client in clients:
        if id_client_transmitter == client.id_client:
            return client
    return None


def register(package_type, id_client_transmitter, id_client_communication, data, address, sock, clients, server):
    logging.info(
        f"REGISTER: id_client_transmitter: {id_client_transmitter}; id_client communication: {id_client_communication}; data: {data}")
    client = check_client(id_client_transmitter, clients)

    if id_client_communication == "0000000000" and data == "" and client.state == "DISCONNECTED":
        random_number = str(randint(1000000000, 9999999999))

        # Open new UDP port
        new_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        newport = server.udp_port + 1
        new_sock.bind((HOST, newport))

        bytes_sent = new_sock.sendto(pack_pdu('a1', server.id_server, random_number, newport), address)
        logging.info(f"PDU REG_ACK sent: {pack_pdu('a1', server.id_server, random_number, newport)}")
        logging.info(f"Bytes sent: {bytes_sent} to address {address}")
        client.state = "WAIT_INFO"
        logging.info(f"Dispositiu {id_client_transmitter} passa a l'estat: {client.state}")

        input_sock = [new_sock]
        for i in range(z):
            input_ready, output_ready, except_ready = select(input_sock, [], [], t)

            if input_ready:
                logging.info(f"INPUT_READY")
                package_type, id_client_transmitter, id_client_communication_1, data, address = read_udp(sock)
                logging.info(f"ID COMMUNICATION: {id_client_communication_1}")
                logging.info(f"DATA: {data}")
                # Fer canvi client a classe i comprovació dades
                if id_client_transmitter in clients and id_client_communication == server.id_server:
                    logging.info("OK")
                    return

        client.state = "DISCONNECTED"
        logging.info(
            f"Client {id_client_transmitter} passa a l'estat: {client.state} perquè s'ha exhaurit el temps {z}")
        # TANCAR SOCKET ???????
        new_sock.close()

        # TODO

    else:
        logging.info(pack_pdu('a3', server.id_server, "0000000000", "Rebuig de registre"))
        sock.sendto(pack_pdu('a3', server.id_server, "0000000000", "Rebuig de registre"), address)
        client.state = "DISCONNECTED"
        logging.info(f"Client {id_client_transmitter} desconnectat per rebuig de registre: {clients}")


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

    input_socket = [sock_udp, sock_tcp, sys.stdin.fileno()]

    while True:
        input_ready, output_ready, except_ready = select(input_socket, [], [])

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
            elif sock == sys.stdin.fileno():
                sys.stdout.write("HELLOO")
            else:
                logging.info(f"\nUnknown socket: {sock}")


if __name__ == '__main__':
    setup()
