import socket
import sys
import threading
import time
from random import *
import logging

from select import *
from struct import *

HOST = 'localhost'  # Standard loopback interface address (localhost)
z = 2
t = 1
w = 3
m = 3
tcp_counter = 0

# Logging config
logging.basicConfig(format='%(asctime)s - %(levelname)s => %(message)s', datefmt='%H:%M:%S', level=logging.INFO)
logging.basicConfig(format='%(asctime)s - %(levelname)s => %(message)s', datefmt='%H:%M:%S', level=logging.DEBUG)


class Server:
    def __init__(self, id_server, udp_port, tcp_port):
        self.id_server = id_server
        self.udp_port = int(udp_port)
        self.tcp_port = tcp_port


class Client:
    def __init__(self, id_client, state=None, tcp_port=None, elements=None, random_number=None, time_alive=None, counter_alive=None, time_tcp=None):
        self.id_client = id_client
        self.state = state
        self.tcp_port = tcp_port
        self.elements = elements
        self.random_number = random_number
        self.time_alive = time_alive
        self.counter_alive = counter_alive
        self.time_tcp = time_tcp


class PduUdp:
    def __init__(self, packet_type, id_transmitter, id_communication, data):
        self.packet_type = packet_type
        self.id_transmitter = id_transmitter
        self.id_communication = id_communication
        self.data = data


class PduTcp:
    def __init__(self, packet_type, id_transmitter, id_communication, element, value, info):
        self.packet_type = packet_type
        self.id_transmitter = id_transmitter
        self.id_communication = id_communication
        self.element = element
        self.value = value
        self.info = info


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
    client = check_client(pdu_udp.id_transmitter, clients)
    if client is not None:
        if client.state == "DISCONNECTED" and pdu_udp.packet_type == 'a0':
            thread = threading.Thread(target=register, args=(pdu_udp, address, sock, client, server))
            thread.start()
        elif client.state == "REGISTERED" or client.state == "SEND_ALIVE" and pdu_udp.packet_type == 'b0':
            client.counter_alive = time.time()
            thread = threading.Thread(target=handle_alive, args=(pdu_udp, address, client, server))
            thread.start()
        else:
            logging.info("Packet desconegut")
            return


def handle_tcp_packet(sock, clients, server):
    pdu_tcp, conn, address = read_tcp(sock, 127)
    thread = threading.Thread(target=handle_send_data, args=(pdu_tcp, sock, clients, server))
    thread.start()


def read_udp(sock_udp, pdu_udp_bytes):
    response = sock_udp.recvfrom(pdu_udp_bytes)
    data = response[0]
    address = response[1]
    pdu_udp = unpack_pdu_udp(data)
    return pdu_udp, address

    # sock_udp.sendto(data, address)


def read_tcp(sock_tcp, pdu_tcp_bytes):
    conn, address = sock_tcp.accept()
    logging.info(f"Rebuda connexió TCP de {address}")
    global tcp_counter
    tcp_counter = time.time()
    thread = threading.Thread(target=tcp_connexion_limit, args=(address,))
    thread.start()
    data_tcp = conn.recv(pdu_tcp_bytes)
    pdu_tcp = None
    logging.info(data_tcp)
    if data_tcp == bytes():
        # FIX SEND_DATA ERROR (TEST 9)
        sock_tcp.close()
    else:
        tcp_counter = 0
        pdu_tcp = unpack_pdu_tcp(data_tcp)
    return pdu_tcp, conn, address


def tcp_connexion_limit(address):
    if time.time() - tcp_counter > m and tcp_counter != 0:
        logging.info(f"No s'han rebut dades per la comunicació TCP amb ip; {address} en {m} segons")


def pack_pdu_udp(package_type, id_client_transmitter, id_client_communication, data):
    return pack("1s11s11s61s", bytes.fromhex(package_type), bytes(id_client_transmitter, "UTF-8"),
                bytes(str(id_client_communication), "UTF-8"), bytes(str(data), "UTF-8"))


def pack_pdu_tcp(package_type, id_client_transmitter, id_client_communication, element, value, info):
    return pack("1s11s11s8s16s80s", bytes.fromhex(package_type), bytes(id_client_transmitter, "UTF-8"), bytes(str(id_client_communication), "UTF-8"), bytes(str(element), "UTF-8"), bytes(str(value), "UTF-8"), bytes(str(info), "UTF-8"))


def unpack_pdu_udp(pdu):
    package_type, id_client_transmitter, id_client_communication, data = unpack('1s11s11s61s', pdu)
    decoded_package_type = package_type.hex()
    decoded_id_client_transmitter = id_client_transmitter.decode("UTF-8").rstrip('\x00')
    decoded_id_client_communication = id_client_communication.decode("UTF-8").rstrip('\x00')
    decoded_data = decode_bytes(data)
    
    # decoded_data = data.decode("UTF-8").split('\x00', 1)[0]
    # logging.info("-------------------------------- UNPACK PDU -----------------------------")
    # logging.info(f"Package type: {decoded_package_type} -> length: {len(decoded_package_type)}")
    # logging.info(f"id_client trasmitter: {decoded_id_client_transmitter} -> length: {len(decoded_id_client_transmitter)}")
    # logging.info(f"id_client Communication: {decoded_id_client_communication} -> length: {len(decoded_id_client_communication)}")
    # logging.info(f"Data: {decoded_data} -> length: {len(decoded_data)}")
    # logging.info("------------------------------ END UNPACK PDU ---------------------------\n")
    return PduUdp(decoded_package_type, decoded_id_client_transmitter, decoded_id_client_communication, decoded_data)


def unpack_pdu_tcp(pdu):
    package_type, id_client_transmitter, id_client_communication, element, value, info = unpack('1s11s11s8s16s80s', pdu)
    decoded_package_type = package_type.hex()
    decoded_id_client_transmitter = id_client_transmitter.decode("UTF-8").rstrip('\x00')
    decoded_id_client_communication = id_client_communication.decode("UTF-8").rstrip('\x00')
    decoded_element = element.decode("UTF-8").rstrip('\x00')
    decoded_value = decode_bytes(value)            
    decoded_info = decode_bytes(info)

    logging.info("-------------------------------- UNPACK PDU TCP -----------------------------")
    logging.info(f"Package type: {decoded_package_type} -> length: {len(decoded_package_type)}")
    logging.info(f"id_client trasmitter: {decoded_id_client_transmitter} -> length: {len(decoded_id_client_transmitter)}")
    logging.info(f"id_client Communication: {decoded_id_client_communication} -> length: {len(decoded_id_client_communication)}")
    logging.info(f"Element: {decoded_element} -> length: {len(decoded_element)}")
    logging.info(f"Value: {decoded_value} -> length: {len(decoded_value)}")
    logging.info(f"Info: {decoded_info} -> length: {len(decoded_info)}")
    logging.info("------------------------------ END UNPACK PDU TCP ---------------------------\n")
    return PduTcp(decoded_package_type, decoded_id_client_transmitter, decoded_id_client_communication, decoded_element, decoded_value, decoded_info)


def decode_bytes(data):
    decoded_data = ""
    for byte in data:
        if byte == 0:
            break
        else:
            decoded_data += chr(byte)
            logging.debug(f"B: {byte}")
    return decoded_data


def check_client(id_client_transmitter, clients):
    for client in clients:
        if id_client_transmitter == client.id_client:
            return client
    return None


def check_client_reg_info(data, client):
    tcp_port = data.split(',')[0]
    elements = data.split(',')[1]
    
    if tcp_port and elements is not None:
        client.tcp_port = tcp_port
        client.elements = elements.split(';')
        logging.info(f"Afegit tcp port: {client.tcp_port} al client: {client.id_client}")
        logging.info(f"Afegit elements: {client.elements} al client: {client.id_client}\n")


def handle_alive(pdu_udp, address, client, server):
    # Open new UDP port
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.udp_port += 1
    sock.bind((HOST, server.udp_port))

    # input_sock = [sock]
    # input_ready, output_ready, except_ready = select(input_sock, [], [], 2)
    # for sock in input_ready:
    
    logging.info("PAQUET ALIVE REBUT")
    logging.info(f"ID TRANSMITTER: {pdu_udp.id_transmitter}")
    logging.info(f"ID COMMUNICATION: {pdu_udp.id_communication}")
    logging.info(f"DATA: {pdu_udp.data}\n")

    logging.debug(f"Temps client: {client.time_alive} i temps actual: {time.time()}")
    if pdu_udp.id_communication == client.random_number and pdu_udp.data == "" and (time.time() - client.time_alive < w or client.time_alive == 0):
        bytes_sent = sock.sendto(pack_pdu_udp('b0', server.id_server, client.random_number, client.id_client), address)
        logging.info(f"PDU ALIVE sent -> id transmissor: {server.id_server}  id comunicació: {client.random_number} dades: {client.id_client}")
        logging.info(f"Bytes sent: {bytes_sent} to address {address}\n")
        if client.state == "REGISTERED":
            client.state = "SEND_ALIVE"
            client.counter_alive = time.time()
            #Només mirar 3 segons el primer cop
            client.time_alive = 0
    else:
        # MODIFICAR ALIVE_REJ AMD DADES CORRECTES (QUINES SON??)
        logging.info("PAQUET ALIVE INCORRECTE")
        bytes_sent = sock.sendto(pack_pdu_udp('b2', server.id_server, client.random_number, "Rebuig de ALIVE"), address)
        logging.info(f"PDU ALIVE_REJ sent -> id transmissor: {server.id_server}  id comunicació: {client.random_number}  dades: Rebuig de ALIVE")
        logging.info(f"Bytes sent: {bytes_sent} to address {address}\n")
        client.state = "DISCONNECTED"


def handle_send_data(pdu_tcp, sock, clients, server):
    client = check_client(pdu_tcp.id_transmitter, clients)
    client.time_tcp = time.time()
    if client is not None:
        if client.state == "SEND_ALIVE" and pdu_tcp.packet_type == 'c0':
            if pdu_tcp.id_communication == client.random_number:
                correct_element = False
                for element in client.elements:
                    if pdu_tcp.id_communication == client.random_number and pdu_tcp.element == element:
                        correct_element = True
                        logging.info("PAQUET SEND_DATA CORRECTE")
                        # EMMAGATZEMAR DADES A DISC
                        write_data(pdu_tcp, client)
                        
                        logging.info(pack_pdu_tcp('c1', server.id_server, client.random_number, pdu_tcp.element, pdu_tcp.value, client.id_client))
                        logging.info(f"PDU DATA_ACK sent -> id transmissor: {server.id_server}  id comunicació: {client.random_number} element: {pdu_tcp.element} value: {pdu_tcp.value} info: {client.id_client}")
                        bytes_sent = sock.send(pack_pdu_tcp('c1', server.id_server, client.random_number, pdu_tcp.element, pdu_tcp.value, client.id_client))
                        logging.info(f"Bytes sent: {bytes_sent}\n")

                if correct_element is False:
                    logging.info("PAQUET SEND_DATA INCORRECTE -> ELEMENT ERRONI")
                    # VALUE INCORRECTE????????
                    sock.send(pack_pdu_tcp('c3', server.id_server, pdu_tcp.id_communication, pdu_tcp.element, pdu_tcp.value, "Element no pertany al dispositiu"))
                    logging.info(
                        f"PDU DATA_REJ sent -> id transmissor: {server.id_server}  id comunicació: {pdu_tcp.id_communication} element: {pdu_tcp.element} value: {pdu_tcp.value} info: Element no pertany al dispositiu")
            else:
                logging.info("PAQUET SEND_DATA INCORRECTE -> ID COMUNICACIÓ ERRONI")
                sock.send(pack_pdu_tcp('c3', server.id_server, "0000000000", pdu_tcp.element, pdu_tcp.value, "Error identificació dispositiu"))
                logging.info(f"PDU DATA_REJ sent -> id transmissor: {server.id_server}  id comunicació: '0000000000' element: {pdu_tcp.element} value: {pdu_tcp.value} info: Error identificació dispositiu")
    else:
        logging.info("PAQUET SEND_DATA INCORRECTE -> ID TRANSMISSIÓ ERRONI")
        sock.send(pack_pdu_tcp('c3', server.id_server, "0000000000", "", "", "Dispositiu no autoritzat"))
        logging.info(f"PDU DATA_REJ sent -> id transmissor: {server.id_server}  id comunicació: '0000000000' element: '' value: '' info: Dispositiu no autoritzat")


def write_data(pdu_tcp, client):
    f = open(client.id_client + ".data", "a")
    f.write(pdu_tcp.info + ";" + pdu_tcp.packet_type + ";" + pdu_tcp.element + ";" + pdu_tcp.value + "\n")
    f.close()


def register(pdu_udp, address, sock, client, server):
    logging.info(
        f"REGISTER: id_client_transmitter: {pdu_udp.id_transmitter}; id_client communication: {pdu_udp.id_communication}; data: {pdu_udp.data}")


    if pdu_udp.id_communication == "0000000000" and pdu_udp.data == "" and client.state == "DISCONNECTED":
        client.random_number = str(randint(1000000000, 9999999999))

        # Open new UDP port
        new_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server.udp_port += 1
        new_sock.bind((HOST, server.udp_port))

        bytes_sent = new_sock.sendto(pack_pdu_udp('a1', server.id_server, client.random_number, server.udp_port), address)
        logging.info(f"PDU REG_ACK sent -> id transmissor: {server.id_server}  id comunicació: {client.random_number}  dades: {server.udp_port}")
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

                check_client_reg_info(pdu_udp.data, client)

                if client is not None:
                    if None not in (client.tcp_port, client.elements) and pdu_udp.id_communication == client.random_number:
                        logging.info("Paquet REG_INFO CORRECTE")
                        bytes_sent = new_sock.sendto(pack_pdu_udp('a5', server.id_server, client.random_number, server.tcp_port), address)
                        logging.info(f"PDU INFO_ACK sent -> id transmissor: {server.id_server}  id comunicació: {client.random_number}  dades: {server.tcp_port}")
                        logging.info(f"Bytes sent: {bytes_sent} to address {address}")
                        client.state = "REGISTERED"
                        client.time_alive = time.time()
                        logging.info(f"Dispositiu {pdu_udp.id_transmitter} passa a l'estat: {client.state}\n")

                    else:
                        logging.info("Paquet REG_INFO INCORRECTE")
                        bytes_sent = new_sock.sendto(pack_pdu_udp('a6', server.id_server, client.random_number, "Error en packet addicional de registre"), address)
                        logging.info(f"PDU INFO_ACK sent -> id transmissor: {server.id_server}  id comunicació: {client.random_number}  dades: 'Error en packet addicional de registre'")
                        logging.info(f"Bytes sent: {bytes_sent} to address {address}")
                        if client is not None:
                            client.state = "DISCONNECTED"
                            logging.info(f"Dispositiu {pdu_udp.id_transmitter} passa a l'estat: {client.state}\n")

        if client.state != "REGISTERED" and client.state != "SEND_ALIVE":
            logging.info(f"{client.state}")
            client.state = "DISCONNECTED"
            logging.info(f"Client {pdu_udp.id_transmitter} passa a l'estat: {client.state} perquè s'ha exhaurit el temps {z}")
        new_sock.close()

    else:
        logging.debug(pack_pdu_udp('a3', server.id_server, "0000000000", "Error en els camps del paquet de registre"))
        sock.sendto(pack_pdu_udp('a3', server.id_server, "0000000000", "Error en els camps del paquet de registre"), address)
        if client is not None:
            client.state = "DISCONNECTED"
            logging.info(f"Client {pdu_udp.id_transmitter} desconnectat error en els camps del paquet de registre: {client.state}")
        return


def check_3_alive(clients):
    for client in clients:
        if client.state == "REGISTERED" and time.time() - client.time_alive > 3 and client.time_alive != 0:
            client.state = "DISCONNECTED"
            logging.info(f"Dispositiu {client.id_client} no ha rebut el primer ALIVE en 3 segons")
        if client.state == "SEND_ALIVE" and time.time() - client.counter_alive > 7:
            client.state = "DISCONNECTED"
            logging.info(f"Client {client.id_client} desconnectat per no enviar 3 ALIVE consecutius")


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
    
        input_ready, output_ready, except_ready = select(input_socket, [], [], 0)

        for sock in input_ready:
            if sock == sock_udp:
                logging.info("Rebut paquet UDP, creat procés per atendre'l")
                handle_udp_packet(sock, clients, server)
            elif sock == sock_tcp:
                logging.info("Rebut paquet TCP, creat procés per atendre'l")
                handle_tcp_packet(sock, clients, server)
                # Replica UDP no usable
                # package_type, id_client_transmitter, id_client_communication, data, conn = read_tcp(sock)
                # register(package_type, id_client_transmitter, id_client_communication, data, conn, clients, server)
            elif sock == sys.stdin.fileno():
                sys.stdout.write("HELLOO")
            else:
                logging.info(f"\nUnknown socket: {sock}")
                
        thread = threading.Thread(target=check_3_alive, args=(clients,))
        thread.start()


if __name__ == '__main__':
    setup()
