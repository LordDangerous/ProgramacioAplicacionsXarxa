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
w = 3

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


class PduUdp:
    def __init__(self, packet_type, id_transmitter, id_communication, data):
        self.packet_type = packet_type
        self.id_transmitter = id_transmitter
        self.id_communication = id_communication
        self.data = data


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
    pdu_udp, address = read_udp(sock, 84)
    if pdu_udp.packet_type == 'a0':
        thread = threading.Thread(target=register, args=(pdu_udp, address, sock, clients, server))
        thread.start()
    elif pdu_udp.packet_type == 'b0':
        thread = threading.Thread(target=handle_alive, args=(clients, server))
        thread.start()
    else:
        logging.info("Packet desconegut")
        return


def read_udp(sock_udp, pdu_udp_bytes):
    response = sock_udp.recvfrom(pdu_udp_bytes)
    data = response[0]
    address = response[1]
    pdu_udp = unpack_pdu(data)
    return pdu_udp, address

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
            logging.debug(f"B: {byte}")
    # decoded_data = data.decode("UTF-8").split('\x00', 1)[0]
    logging.info("-------------------------------- UNPACK PDU -----------------------------")
    logging.info(f"Package type: {decoded_package_type} length: {len(decoded_package_type)}")
    logging.info(f"id_client trasmitter: {decoded_id_client_transmitter} length: {len(decoded_id_client_transmitter)}")
    logging.info(
        f"id_client Communication: {decoded_id_client_communication}; length: {len(decoded_id_client_communication)}")
    logging.info(f"Data: {decoded_data}; length: {len(decoded_data)}")
    logging.info("------------------------------ END UNPACK PDU ---------------------------\n")
    return PduUdp(decoded_package_type, decoded_id_client_transmitter, decoded_id_client_communication, decoded_data)


def check_client(id_client_transmitter, clients):
    for client in clients:
        if id_client_transmitter == client.id_client:
            return client
    return None


def check_client_reg_info(data, client):
    tcp_port = elements = None
    tcp_port = data.split(',')[0]
    elements = data.split(',')[1]
    
    if tcp_port and elements is not None:
        client.tcp_port = tcp_port
        client.elements = elements.split(';')
        logging.info(f"Afegit tcp port: {client.tcp_port} al client: {client.id_client}")
        logging.info(f"Afegit elements: {client.elements} al client: {client.id_client}\n")


def handle_alive(pdu_udp, clients, server):
    # Open new UDP port
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.udp_port += 1
    sock.bind((HOST, server.udp_port))

    input_sock = [sock]
    for _ in range(w):
        print(f"bucle for {w}")
        input_ready, output_ready, except_ready = select(input_sock, [], [], 2)

        for sock in input_ready:
            pdu_udp, address = read_udp(new_sock, 84)
            logging.info("PAQUET ALIVE REBUT")
            logging.info(f"ID TRANSMITTER: {pdu_udp.id_transmitter}")
            logging.info(f"ID COMMUNICATION: {pdu_udp.id_communication}")
            logging.info(f"DATA: {pdu_udp.data}\n")

            client = check_client(pdu_udp.id_transmitter, clients)

            if client is not None:
                if pdu_udp.id_communication == server.id_server and pdu_udp.data == "":
                    logging.info("PAQUET ALIVE CORRECTE")
                else:
                    logging.info("PAQUET ALIVE INCORRECTE")
            return


def register(pdu_udp, address, sock, clients, server):
    logging.info(
        f"REGISTER: id_client_transmitter: {pdu_udp.id_transmitter}; id_client communication: {pdu_udp.id_communication}; data: {pdu_udp.data}")
    client = check_client(pdu_udp.id_transmitter, clients)

    if client is not None:
        if pdu_udp.id_communication == "0000000000" and pdu_udp.data == "" and client.state == "DISCONNECTED":
            random_number = str(randint(1000000000, 9999999999))

            # Open new UDP port
            new_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            server.udp_port += 1
            new_sock.bind((HOST, server.udp_port))

            bytes_sent = new_sock.sendto(pack_pdu('a1', server.id_server, random_number, server.udp_port), address)
            logging.info(f"PDU REG_ACK sent -> id transmissor: {server.id_server}  id comunicació: {random_number}  dades: {server.udp_port}")
            logging.info(f"Bytes sent: {bytes_sent} to address {address}")
            client.state = "WAIT_INFO"
            logging.info(f"Dispositiu {pdu_udp.id_transmitter} passa a l'estat: {client.state}\n")

            input_sock = [new_sock]

            for i in range(z):
                input_ready, output_ready, except_ready = select(input_sock, [], [], t)

                for new_sock in input_ready:
                    pdu_udp, address = read_udp(new_sock, 84)
                    logging.info(f"ID TRANSMITTER: {pdu_udp.id_transmitter}")
                    logging.info(f"ID COMMUNICATION: {pdu_udp.id_communication}")
                    logging.info(f"DATA: {pdu_udp.data}\n")

                    client = check_client(pdu_udp.id_transmitter, clients)
                    check_client_reg_info(pdu_udp.data, client)
                    
                    
                    if client is not None:
                        if None not in (client.tcp_port, client.elements) and pdu_udp.id_communication == random_number:
                            logging.info("Paquet REG_INFO CORRECTE")
                            bytes_sent = new_sock.sendto(pack_pdu('a5', server.id_server, random_number, server.tcp_port), address)
                            logging.info(f"PDU INFO_ACK sent -> id transmissor: {server.id_server}  id comunicació: {random_number}  dades: {server.tcp_port}")
                            logging.info(f"Bytes sent: {bytes_sent} to address {address}")
                            client.state = "REGISTERED"
                            logging.info(f"Dispositiu {pdu_udp.id_transmitter} passa a l'estat: {client.state}\n")

                        else:
                            logging.info("Paquet REG_INFO INCORRECTE")
                            bytes_sent = new_sock.sendto(pack_pdu('a6', server.id_server, random_number, "Error en packet addicional de registre"), address)
                            logging.info(f"PDU INFO_ACK sent -> id transmissor: {server.id_server}  id comunicació: {random_number}  dades: 'Error en packet addicional de registre'")
                            logging.info(f"Bytes sent: {bytes_sent} to address {address}")
                            if client is not None:
                                client.state = "DISCONNECTED"
                                logging.info(f"Dispositiu {pdu_udp.id_transmitter} passa a l'estat: {client.state}\n")

            if client.state != "REGISTERED":
                client.state = "DISCONNECTED"
                logging.info(f"Client {pdu_udp.id_transmitter} passa a l'estat: {client.state} perquè s'ha exhaurit el temps {z}")
            new_sock.close()

        else:
            logging.debug(pack_pdu('a3', server.id_server, "0000000000", "Error en els camps del paquet de registre"))
            sock.sendto(pack_pdu('a3', server.id_server, "0000000000", "Error en els camps del paquet de registre"), address)
            if client is not None:
                client.state = "DISCONNECTED"
                logging.info(f"Client {pdu_udp.id_transmitter} desconnectat error en els camps del paquet de registre: {client.state}")
            return
    return


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
                logging.info("Rebut paquet UDP, creat procés per atendre'l")
                handle_udp_packet(sock, clients, server)
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
