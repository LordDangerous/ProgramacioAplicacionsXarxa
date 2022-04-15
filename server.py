import socket
import threading
import time
import signal
from random import *
import logging
import sys
import os
from select import *
from struct import *


z = 2
t = 1
w = 3
m = 3
v = 2


# Definir les variables HOST, en el nostre cas sempre localhost, server_file, que contindrà el nom de l'arxiu per defecte o 
# l'especificat per l'usuari per terminal, de la mateixa forma que la variable global database_file per la base de dades. 
# quit servirà per saber si l'usuari ha introduït la comanda "quit" per terminal i fer el procediment necessari
HOST = 'localhost'
server_file = "server.cfg"
database_file = "bbdd_dev.dat"
quit = False


# Ús del paquet logging de la llibreria estàndard de Python per definir els tres nivells de prints
logging.basicConfig(format='%(asctime)s - %(levelname)s => %(message)s', datefmt='%H:%M:%S', level=logging.INFO)
logging.basicConfig(format='%(asctime)s - %(levelname)s => %(message)s', datefmt='%H:%M:%S', level=logging.DEBUG)
logging.basicConfig(format='%(asctime)s - %(levelname)s => %(message)s', datefmt='%H:%M:%S', level=logging.ERROR)


class Server:
    def __init__(self, id_server, udp_port, tcp_port):
        self.id_server = id_server
        self.udp_port = int(udp_port)
        self.tcp_port = tcp_port


class Client:
    def __init__(self, id_client, state=None, tcp_port=None, elements=None, random_number=None, time_alive=None, counter_alive=None, address=None):
        self.id_client = id_client
        self.state = state
        self.tcp_port = tcp_port
        self.elements = elements
        self.random_number = random_number
        self.time_alive = time_alive
        self.counter_alive = counter_alive
        self.address = address

    # Creació del mètode reset perquè si un client es desconnecta borrar totes les seves dades associades
    def reset(self):
        self.state = "DISCONNECTED"
        self.tcp_port = None
        self.elements = None
        self.random_number = None
        self.time_alive = None
        self.counter_alive = None
        self.address = None


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


#Funció encarregada de llegir els arguments introduïts per l'usuari a l'inici de l'execució del programa
def parse_args():
    args = sys.argv[1:]
    length = len(args)
    if length > 0:
        for i in range(length):
            if args[i] == "-d":
                # Si s'ha introduït l'opció "-d" canviem el nivell de prints a DEBUG
                level = logging.DEBUG
                logger = logging.getLogger()
                logger.setLevel(level)
            elif args[i] == "-c":
                if length > i + 1:
                    global server_file
                    server_file = args[i+1]
                else:
                    logging.error(f"Arxiu de configuració no especificat")
                    exit()
            elif args[i] == "-u":
                if length > i + 1:
                    global database_file
                    database_file = args[i+1]
                else:
                    logging.error("Base de dades de dispositius no especificada")
                    exit()


# Funció per llegir l'arxiu de configuració
def read_file():
    id_server = udp_port = tcp_port = None
    # Assegurar que s'ha introduït un arxiu correcte
    try:
        f = open(server_file, "r")
    except IOError:
        logging.error(f"No es pot obrir l'arxiu de configuració: {server_file}")
        exit()
    read = f.read()
    for line in read.splitlines():
        if 'Id =' in line:
            id_server = line.split('= ', 1)[1]
        elif 'UDP-port =' in line:
            udp_port = line.split('= ')[1]
        elif 'TCP-port =' in line:
            tcp_port = line.split('= ')[1]
    # Revisar que apareixen tots els camps necessaris a l'arxiu de configuració i que tenen un valor
    if None not in (id_server, udp_port, tcp_port):
        return Server(id_server, udp_port, tcp_port)
    else:
        logging.error("No es pot obtenir l'identificador del servidor de l'arxiu de configuració")
        exit()


# Funció encarregada de llegir l'arxiu que conté la base de dades
def read_database():
    try:
        f = open(database_file, "r")
    except IOError:
        logging.info(f"No es pot obrir la base de dades de dispositius: {database_file}")
        exit()
    read = f.read()
    clients = []
    for line in read.splitlines():
        # Creació d'una llista dels clients
        clients.append(Client(line, "DISCONNECTED"))
    show_table(clients)
    return clients


# Funció auxiliar per mostrar per pantalla l'estat del client passat com a paràmetre
def print_client_state(client):
    logging.info(f"Dispositiu {client.id_client} passa a l'estat {client.state}")


# Funció auxiliar per convertir el tipus de paquet a cadena de caràcters per poder mostrar-ho per pantalla
def packet_type_converter(packet_type):
    if packet_type == 'a0':
        packet_type_s = "REG_REQ"
    elif packet_type == 'a1':
        packet_type_s = "REG_ACK"
    elif packet_type == 'a2':
        packet_type_s = "REG_NACK"
    elif packet_type == 'a3':
        packet_type_s = "REG_REJ"
    elif packet_type == 'a4':
        packet_type_s = "REG_INFO"
    elif packet_type == 'a5':
        packet_type_s = "INFO_ACK"
    elif packet_type == 'a6':
        packet_type_s = "INFO_NACK"
    elif packet_type == 'a7':
        packet_type_s = "INFO_REJ"
    elif packet_type == 'b0':
        packet_type_s = "ALIVE"
    elif packet_type == 'b1':
        packet_type_s = "ALIVE_NACK"
    elif packet_type == 'b2':
        packet_type_s = "ALIVE_REJ"
    elif packet_type == 'c0':
        packet_type_s = "SEND_DATA"
    elif packet_type == 'c1':
        packet_type_s = "DATA_ACK"
    elif packet_type == 'c2':
        packet_type_s = "DATA_NACK"
    elif packet_type == 'c3':
        packet_type_s = "DATA_REJ"
    elif packet_type == 'c4':
        packet_type_s = "SET_DATA"
    elif packet_type == 'c5':
        packet_type_s = "GET_DATA"
    return packet_type_s


# Funció encarregada d'atendre la connexió de tipus UDP
def handle_udp_packet(sock, clients, server):
    pdu_udp, address = read_udp(sock, 84)
    client = check_client(pdu_udp.id_transmitter, clients)
    # Comprovar que el paquet conté un id transmissor
    if client is not None:
        client.address = address[0]
        if client.state == "DISCONNECTED":
            if pdu_udp.packet_type == 'a0':
                # Iniciar el procés de registre
                thread = threading.Thread(target=register, args=(pdu_udp, address, sock, client, server))
                thread.start()
            else:
                logging.debug(f"Rebut paquet: {packet_type_converter(pdu_udp.packet_type)} del dispositiu {pdu_udp.id_transmitter} en estat: {client.state}")
        elif client.state == "REGISTERED" or client.state == "SEND_ALIVE":
            if pdu_udp.packet_type == 'b0':
                # Actualitzar el temps per desconnectar el client si no es reben 3 ALIVE consecutius
                client.counter_alive = time.monotonic()
                # Iniciar el procés de manteniment de comunicació periòdica
                thread = threading.Thread(target=handle_alive, args=(sock, pdu_udp, address, client, server))
                thread.start()
            else:
                logging.debug(f"Rebut paquet: {packet_type_converter(pdu_udp.packet_type)} del dispositiu {pdu_udp.id_transmitter} en estat: {client.state}")
                if client.state != "DISCONNECTED":
                    client.state = "DISCONNECTED"
                    print_client_state(client)
        else:
            logging.info("Packet desconegut")
            return
    else:
        logging.info(f"Rebutjat paquet REG_REQ. Id.: {pdu_udp.id_transmitter} no autoritzat")
        send_udp(sock, 'a3', server.id_server, "0000000000", "Dispositiu no autoritzat en el sistema", address)
        return


# Funció encarregada d'atendre la connexió TCP dels clients
def handle_tcp_packet(sock, clients, server):
    pdu_tcp, conn, address = read_tcp(sock, 127)
    client = check_client(pdu_tcp.id_transmitter, clients)
    if client is not None:
        if pdu_tcp is not None:
            if pdu_tcp.packet_type == 'c0':
                handle_send_data(pdu_tcp, conn, client, server)
            else:
                logging.debug(f"Rebut paquet: {packet_type_converter(pdu_tcp.packet_type)} del dispositiu {pdu_tcp.id_transmitter} en una connexió TCP")
                client.state = "DISCONNECTED"
                print_client_state(client)
    else:
        logging.info(f"Rebut paquet incorrecte. Dispositiu: Id. transmissor: {pdu_tcp.id_transmitter} (no autoritzat)")
        send_tcp(conn, 'c3', server.id_server, "0000000000", "", "", "Dispositiu no autoritzat")
    sock.close()
    return


# Funció encarregada de comprovar la comanda introduïda i respondre de la forma necessaria
def handle_commands(sock, server_input, clients, server):
    # Dividir la comanda introduïda ("split") per cada caràcter " " trobat
    input_splitted = server_input.split()
    for i, word in enumerate(input_splitted):
        input_splitted[i] = word

    i = len(input_splitted)
    server_command = input_splitted[0]
    if server_command == "set":
        if i == 4:
            id_client = input_splitted[1]
            id_element = input_splitted[2]
            new_value = input_splitted[3]
            print(id_element[-1])
            if id_element[-1] == 'I':
                # Si s'ha introduït correctament tota la comanda set iniciem un procés per encarregar-se 
                thread = threading.Thread(target=handle_set_and_get, args=(id_client, id_element, new_value, clients, server, server_command))
                thread.start()
            else:
                logging.info(f"L'element anomenat: {id_element} és un sensor i no permet establir el seu valor")
        else:
            logging.info("Error de sintàxi. (set <nom_contr.> <element> <valor>)")
    elif server_command == "get":
        if i == 3:
            id_client = input_splitted[1]
            id_element = input_splitted[2]
            thread = threading.Thread(target=handle_set_and_get, args=(id_client, id_element, None, clients, server, server_command))
            thread.start()
        else:
            logging.info("Error de sintàxi. (set <nom_contr.> <element>)")
    elif server_command == "list":
        show_table(clients)
    elif server_command == "quit":
        # Canviem el valor de la variable global quit a cert i d'aquesta forma saber que s'ha introduït la comanda
        global quit
        quit = True
    else:
        logging.info(f"Comanda incorrecta {input_splitted[0]}")


# Funció auxiliar per mostrar per terminal l'estat actual de tots els clients de la base de dades i la seva informació
def show_table(clients):
    titles_list = ["-ID. DISP-", "-ID.COM.-", "----- IP -----", "---- ESTAT ----", "---------- ELEMENTS ----------"]
    clients_list = []
    for client in clients:
        clients_i = []
        clients_i.append(client.id_client)
        if client.random_number is not None:
            clients_i.append(client.random_number)
        else:
            clients_i.append('')
        if client.address is not None:
            clients_i.append(client.address)
        else:
            clients_i.append('')
        clients_i.append(client.state)
        if client.elements is not None:
            elements = ""
            for i, element in enumerate(client.elements):
                if i < len(client.elements) - 1:
                    elements += '{};'.format(element)
                else:
                    elements += '{}'.format(element)
            clients_i.append(elements)
        else:
            clients_i.append('')
        clients_list.append(clients_i)

    # Utilitzar la funció format per mostrar correctament (alineament a l'esquerra i amb una mida de 17) la taula creada
    row_format = "{:<17}" * (len(titles_list))
    print(row_format.format(*titles_list))
    for team, row in zip(titles_list, clients_list):
        print(row_format.format(*row))


# Funció per enviar informació als clients mitjaçant els paquets SET_DATA i GET_DATA
def handle_set_and_get(id_client, id_element, new_value, clients, server, command):
    client = check_client(id_client, clients)
    if client is not None:
        # Creació d'una nova connexió TCP pel port del client subministrat durant la fase de registre
        tcp_connexion = (HOST, client.tcp_port)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.connect(tcp_connexion)
        except socket.error:
            client.state = "DISCONNECTED"
            print_client_state(client)
        if command == "get":
            # Enviar paquet GET_DATA
            send_tcp(sock, 'c5', server.id_server, client.random_number, id_element, "", id_client)
        else:
            # Enviar paquet SET_DATA
            send_tcp(sock, 'c4', server.id_server, client.random_number, id_element, new_value, id_client)

        pdu_tcp = read_set_get_answer(sock, 127)
        if pdu_tcp is not None:
            if pdu_tcp.packet_type == 'c1':
                if pdu_tcp.id_transmitter == id_client:
                    if pdu_tcp.id_communication == client.random_number:
                        correct_element = False
                        # Iterar per tots els elements del client per veure si es correcte
                        for element in client.elements:
                            if pdu_tcp.element == element:
                                correct_element = True
                                # Si totes les comprovacions es cumpleixen escriure el resultat
                                write_data(pdu_tcp, client)
                        if correct_element is False:
                            logging.info(f"Error en les dades d'identificació de l'element: {id_element} del dispositiu: {id_client} (rebut element: {pdu_tcp.element})")
                    else:
                        logging.info(f"Error en les dades d'identificació del dispositiu: {client.random_number} (rebut id. com.: {pdu_tcp.id_communication}")
                        client.state = "DISCONNECTED"
                        print_client_state(client)

                else:
                    logging.info(f"Error en les dades d'identificació del dispositiu: {id_client} (rebut id: {pdu_tcp.id_transmitter}, id. com.: {pdu_tcp.id_communication}")
                    client.state = "DISCONNECTED"
                    print_client_state(client)
            elif pdu_tcp.packet_type == 'c2':
                logging.debug(f"Paquet DATA_NACK rebut")
            elif pdu_tcp.packet_type == 'c3':
                logging.debug(f"Paquet DATA_REJ rebut")
                client.state = "DISCONNECTED"
                print_client_state(client)
        else:
            return
    else:
        logging.info(f"Client desconegut")
    # Tancar la connexió TCP a l'acabar l'enviament o petició d'informació
    sock.close()


# Funció encarregada de llegir continuament per la connexió TCP passada per paràmetre per veure si ha arribat alguna
# informació durant els m segons. Si és així, es retorna la pdu de tipus TCP, sinó, s'informa per terminal i es retorna None
def read_set_get_answer(sock, pdu_tcp_bytes):
    data_tcp = sock.recv(pdu_tcp_bytes)
    pdu_tcp = None
    timer = time.monotonic()
    tcp_counter = time.monotonic()
    while len(data_tcp) == 0 and tcp_counter - timer <= m:
        tcp_counter = time.monotonic()
        data_tcp = sock.recv(pdu_tcp_bytes)
    if len(data_tcp) == pdu_tcp_bytes:
        pdu_tcp = unpack_pdu_tcp(data_tcp, pdu_tcp_bytes)
    else:
        logging.info(f"No s'han rebut dades per la comunicació TCP en {m} segons")
    return pdu_tcp


# Funció auxiliar encarregada de mirar si hi ha informació al socket i fer el split entre les dades i l'adreça del client
def read_udp(sock_udp, pdu_udp_bytes):
    response = sock_udp.recvfrom(pdu_udp_bytes)
    data = response[0]
    address = response[1]
    pdu_udp = unpack_pdu_udp(data, pdu_udp_bytes)
    return pdu_udp, address


# Funció molt semblant a read_set_get_answer però en aquest cas esperarà rebre informació dels clients per una altra connexió TCP.
# De la mateixa forma, si no s'ha rebut cap informació en m segons s'informarà per terminal
def read_tcp(sock_tcp, pdu_tcp_bytes):
    conn, address = sock_tcp.accept()
    pdu_tcp = None
    timer = time.monotonic()
    tcp_counter = time.monotonic()
    data_tcp = conn.recv(pdu_tcp_bytes)
    while len(data_tcp) == 0 and tcp_counter - timer <= m:
        tcp_counter = time.monotonic()
        data_tcp = conn.recv(pdu_tcp_bytes)
    if len(data_tcp) == pdu_tcp_bytes:
        pdu_tcp = unpack_pdu_tcp(data_tcp, pdu_tcp_bytes)
    else:
        logging.debug(f"No s'han rebut dades per la comunicació TCP amb ip: {address[0]} en {m} segons")
    return pdu_tcp, conn, address


# Funció auxiliar encarregada de passar a bytes els diferents paràmetres, i de forma diferent cadascun depenent del tipus d'informació
# que contenen, així com fer ús de la funció pack, amb el nombre de bytes definit per cada paràmetre, per aconseguir un objecte de tipus
# bytes que contigui tota la informació. Es defineixen, per la pdu UDP, 1bytes pel tipus de paquet, 11 bytes per l'id transmissor,
# 11 bytes per l'id communicació i 61 per les dades (primer paràmetre de la funció pack)
def pack_pdu_udp(package_type, id_client_transmitter, id_client_communication, data):
    return pack("1s 11s 11s 61s", bytes.fromhex(package_type), bytes(id_client_transmitter, "UTF-8"),
                bytes(str(id_client_communication), "UTF-8"), bytes(str(data), "UTF-8"))


# Funció auxiliar idèntica a pack_pdu_udp, però per una pdu de tipus TCP, amb els bytes pertinents per cada camp
def pack_pdu_tcp(package_type, id_client_transmitter, id_client_communication, element, value, info):
    return pack("1s 11s 11s 8s 16s 80s", bytes.fromhex(package_type), bytes(id_client_transmitter, "UTF-8"), bytes(str(id_client_communication), "UTF-8"), bytes(str(element), "UTF-8"), bytes(str(value), "UTF-8"), bytes(str(info), "UTF-8"))


# Funció encarregada de desempaquetar els bytes rebuts per un socket de tipus UDP, mitjançant la funció unpack i els bytes dels diferents camps,
# i guardar en un objecte de tipus PduUdp els diferents valors
def unpack_pdu_udp(pdu, bytes_received):
    package_type, id_client_transmitter, id_client_communication, data = unpack('1s11s11s61s', pdu)
    decoded_package_type = package_type.hex()
    decoded_id_client_transmitter = id_client_transmitter.decode("UTF-8").rstrip('\x00')
    decoded_id_client_communication = id_client_communication.decode("UTF-8").rstrip('\x00')
    decoded_data = decode_bytes(data)

    packet_type_s = packet_type_converter(decoded_package_type)
    logging.debug(f"Rebut -> bytes: {bytes_received}  paquet: {packet_type_s}  id transmissor: {decoded_id_client_transmitter}  id comunicació: {decoded_id_client_communication}  dades: {decoded_data}")
    return PduUdp(decoded_package_type, decoded_id_client_transmitter, decoded_id_client_communication, decoded_data)


# Funció per desempaquetar bytes rebuts per connexions TCP i guardar la informació en un objecte de tipus PduTcp
def unpack_pdu_tcp(pdu, bytes_received):
    package_type, id_client_transmitter, id_client_communication, element, value, info = unpack('1s11s11s8s16s80s', pdu)
    decoded_package_type = package_type.hex()
    decoded_id_client_transmitter = id_client_transmitter.decode("UTF-8").rstrip('\x00')
    decoded_id_client_communication = id_client_communication.decode("UTF-8").rstrip('\x00')
    decoded_element = element.decode("UTF-8").rstrip('\x00')
    decoded_value = decode_bytes(value)            
    decoded_info = decode_bytes(info)

    packet_type_s = packet_type_converter(decoded_package_type)
    logging.debug(f"Rebut -> bytes: {bytes_received}  paquet: {packet_type_s}  id transmissor: {decoded_id_client_transmitter}  id comunicació: {decoded_id_client_communication}  element: {decoded_element}  valor: {decoded_value}  info: {decoded_info}")
    return PduTcp(decoded_package_type, decoded_id_client_transmitter, decoded_id_client_communication, decoded_element, decoded_value, decoded_info)


# Funció auxiliar encarregada de mirar byte a byte la informació del paràmetre i concatenar-la, passada a càracter, 
# en una string, si el byte no és zero
def decode_bytes(data):
    decoded_data = ""
    for byte in data:
        if byte == 0:
            break
        else:
            decoded_data += chr(byte)
    return decoded_data


# Funció auxiliar que comprova que l'id transmissor rebut correspon a un client del servidor
def check_client(id_client_transmitter, clients):
    for client in clients:
        if id_client_transmitter == client.id_client:
            return client
    return None


# Funció auxiliar per fer un "split" de la informació rebuda en el paquet REG_INFO i guardar-la al client
def check_client_reg_info(data, client):
    tcp_port = data.split(',')[0]
    elements = data.split(',')[1]
    
    if tcp_port and elements is not None:
        client.tcp_port = int(tcp_port)
        client.elements = elements.split(';')
        logging.debug(f"Afegit tcp port: {client.tcp_port} al client: {client.id_client}")
        logging.debug(f"Afegit elements: {client.elements} al client: {client.id_client}\n")


# Funció encarregada d'atendre el manteniment de la comunicació periòdica amb els clients
def handle_alive(sock, pdu_udp, address, client, server):
    if pdu_udp.id_communication == client.random_number and pdu_udp.data == "" and (time.monotonic() - client.time_alive < w or client.time_alive == 0):
        send_udp(sock, 'b0', server.id_server, client.random_number, client.id_client, address)
        if client.state == "REGISTERED":
            client.state = "SEND_ALIVE"
            print_client_state(client)
            # Només mirar 3 segons pel primer ALIVE a rebre
            client.time_alive = 0
    else:
        logging.debug("Rebut paquet: ALIVE del dispositiu {client.id_client} amb dades incorrectes")
        send_udp(sock, 'b2', server.id_server, client.random_number, "Error en dades del dispositiu", address)
        client.state = "DISCONNECTED"
        print_client_state(client)


# Funció encarregada de comprovar el paquet SEND_DATA enviat pel client, guardar la informació si tot és correcte, i contestar amb el paquet pertinent
def handle_send_data(pdu_tcp, conn, client, server):
    if client.state == "SEND_ALIVE" and pdu_tcp.packet_type == 'c0':
        if pdu_tcp.id_communication == client.random_number:
            correct_element = False
            for element in client.elements:
                if pdu_tcp.element == element:
                    correct_element = True
                    logging.debug("PAQUET SEND_DATA CORRECTE")
                    # Emmagatzemar les dades si totes les comprovacions són correctes
                    write_data(pdu_tcp, client)

                    send_tcp(conn, 'c1', server.id_server, client.random_number, pdu_tcp.element, pdu_tcp.value, client.id_client)

            if correct_element is False:
                logging.info(f"Rebut paquet incorrecte. Dispositiu: Id: {pdu_tcp.id_transmitter}. Error en el valor del camp element: {pdu_tcp.element}")
                send_tcp(conn, 'c3', server.id_server, pdu_tcp.id_communication, pdu_tcp.element, pdu_tcp.value, "Element no pertany al dispositiu")
        else:
            logging.info(f"Rebut paquet incorrecte. Dispositiu: Id: {pdu_tcp.id_transmitter}. Error en el valor del camp id. comunicació: {pdu_tcp.id_communication}")
            send_tcp(conn, 'c3', server.id_server, "0000000000", pdu_tcp.element, pdu_tcp.value, "Error identificació dispositiu")
            client.state = "DISCONNECTED"
            print_client_state(client)


# Funció auxiliar que obre l'arxiu, o el crea, amb el nom del client i afegeix en una nova línia la informació
def write_data(pdu_tcp, client):
    f = open(client.id_client + ".data", "a")
    f.write(pdu_tcp.info + ";" + pdu_tcp.packet_type + ";" + pdu_tcp.element + ";" + pdu_tcp.value + "\n")
    f.close()


# Funció encarregada d'enviar per una comunicació UDP una informació de tipus bytes
def send_udp(sock, package_type, id_transmitter, id_communication, data, address):
    bytes_sent = sock.sendto(pack_pdu_udp(package_type, id_transmitter, id_communication, data), address)
    packet_type_s = packet_type_converter(package_type)
    logging.debug(f"Enviat -> bytes: {bytes_sent}  paquet: {packet_type_s}  id transmissor: {id_transmitter}  id comunicació: {id_communication}  dades: {data}")


# Funció encarregada d'enviar per una connexió TCP una informació de tipus bytes
def send_tcp(conn, package_type, id_transmitter, id_communication, element, value, info):
    bytes_sent = conn.send(pack_pdu_tcp(package_type, id_transmitter, id_communication, element, value, info))
    packet_type_s = packet_type_converter(package_type)
    logging.debug(f"Enviat -> bytes: {bytes_sent}  paquet: {packet_type_s}  id transmissor: {id_transmitter}  id comunicació: {id_communication}  element: {element}  valor: {value}  info: {info}")


# Funció encarregada de la fase de registre d'un client al servidor
def register(pdu_udp, address, sock, client, server):
    if pdu_udp.id_communication == "0000000000" and pdu_udp.data == "" and client.state == "DISCONNECTED":
        client.random_number = str(randint(1000000000, 9999999999))

        # Obrir un nou port UDP per seguir la fase de registre
        new_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server.udp_port += 1
        try:
            new_sock.bind((HOST, server.udp_port))
        except socket.error as msg:
            logging.error(f"No es pot fer el binding del nou socket UDP (errno: {msg})")
            exit()

        send_udp(sock, 'a1', server.id_server, client.random_number, server.udp_port, address)
        client.state = "WAIT_INFO"
        print_client_state(client)

        input_sock = [new_sock]

        # Mitjançant la funció select (amb el timeout t = 1) i la instrucció for fins a z aconseguim saber si el paquet REG_INFO arriba abans de 
        # z segons al servidor
        for i in range(z):
            # La funció select espera a que un descriptor de fitxer estigui preparat de la llista "input_sock"
            input_ready, output_ready, except_ready = select(input_sock, [], [], t)

            for new_sock in input_ready:
                pdu_udp, address = read_udp(new_sock, 84)              

                if client.id_client == pdu_udp.id_transmitter:
                    if pdu_udp.id_communication == client.random_number:
                        if pdu_udp.data != "":
                            check_client_reg_info(pdu_udp.data, client)
                            send_udp(new_sock, 'a5', server.id_server, client.random_number, server.tcp_port, address)
                            client.state = "REGISTERED"
                            print_client_state(client)
                            # Establir el comptador de temps per rebre el primer paquet ALIVE, ja que el client ha passat a l'estat REGISTERED
                            client.time_alive = time.monotonic()
                            return
                        else:
                            logging.debug(f"Rebut paquet {packet_type_converter(pdu_udp.packet_type)} del dispositiu: {pdu_udp.id_transmitter} sense dades al camp data")
                            send_udp(new_sock, 'a6', server.id_server, client.random_number, "REG_INFO sense dades addicionals", address)
                            client.state = "DISCONNECTED"
                            print_client_state(client)
                            return
                    else:
                        logging.debug(f"Rebut paquet {packet_type_converter(pdu_udp.packet_type)} del dispositiu: {pdu_udp.id_transmitter} amb id comunicació incorrecte")
                        send_udp(new_sock, 'a6', server.id_server, client.random_number, "REG_INFO amb id comunicació incorrecte", address)
                        client.state = "DISCONNECTED"
                        print_client_state(client)
                        return            
                else:
                    logging.debug(f"Rebut paquet {packet_type_converter(pdu_udp.packet_type)} del dispositiu: {pdu_udp.id_transmitter} no autoritzat")
                    send_udp(new_sock, 'a6', server.id_server, client.random_number, "REG_INFO sense dades addicionals", address)
                    client.state = "DISCONNECTED"
                    print_client_state(client)
                    return
                            

        if client.state != "REGISTERED" and client.state != "SEND_ALIVE":
            logging.info(f"S'ha exhaurit el temps z: {z} per rebre el paquet REG_INFO")
            client.state = "DISCONNECTED"
            print_client_state(client)
        new_sock.close()

    else:
        if client is not None:
            logging.info(f"Petició de registre errònia. Dispositiu: Id: {pdu_udp.id_transmitter} , id. comunicació: {pdu_udp.id_communication} , data: {pdu_udp.data}")
            send_udp(sock, 'a3', server.id_server, "0000000000", "Error en els camps del paquet de registre", address)
            client.state = "DISCONNECTED"
            print_client_state(client)
    return


# Funció auxiliar que comprova continuament que els clients enviïn el primer ALIVE dins del temps w i que es rebin 3 ALIVE consecutius
def check_3_alive(clients):
    for client in clients:
        if client.state == "REGISTERED" and time.monotonic() - client.time_alive > w and client.time_alive != 0:
            logging.info(f"Dispositiu {client.id_client} no ha rebut el primer ALIVE en 3 segons")
            client.state = "DISCONNECTED"
            print_client_state(client)
        if client.state == "SEND_ALIVE" and time.monotonic() - client.counter_alive > (v * 3):
            logging.info(f"Client {client.id_client} desconnectat per no enviar 3 ALIVE consecutius")
            client.state = "DISCONNECTED"
            print_client_state(client)


# Funció auxiliar per borrar el contingut emmagatzemat al servidor del client que s'ha desconnectat mitjançant el mètode reset de la classe Client
def reset_client(clients):
    for client in clients:
        if client is not None and client.state == "DISCONNECTED":
            client.reset()


# Handler per detectar si l'usuari ha introduït ctrl + c per aturar el servidor
def handle_SIGINT(signum, frame):
    logging.info("Finalització per ^C")
    exit(1)


# Funció encarregada d'assignar a la senyal SIGINT (ctrl + c) el handler corresponent, cridar a les funcions responsables 
# de mirar els arguments introduïts al executar el programa, llegir l'arxiu de configuració i la base de dades i, sobretot, 
# inicialitzar els sockets UDP, TCP i el descriptor de fitxer per llegir les comandes introduïdes per terminal. A més a més, 
# si la variable global "quit" és certa tancarà les comunicacions i finalitzarà el programa
def setup():
    signal.signal(signal.SIGINT, handle_SIGINT)
    parse_args()
    server = read_file()
    clients = read_database()

    # Inicialització socket UDP
    sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock_udp.bind((HOST, int(server.udp_port)))
    except socket.error as msg:
        logging.error(f"No es pot fer el binding del socket UDP (errno: {msg})")
        exit()

    # Inicialització socket TCP
    sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock_tcp.bind((HOST, int(server.tcp_port)))
    except socket.error as msg:
        logging.error(f"No es pot fer el binding del socket TCP (errno: {msg})")
        exit()
    sock_tcp.listen()

    input_socket = [sock_udp, sock_tcp, sys.stdin.fileno()]

    while True:
    
        input_ready, output_ready, except_ready = select(input_socket, [], [], 0)

        for sock in input_ready:
            if sock == sock_udp:
                logging.debug("Rebut paquet UDP, creat procés per atendre'l")
                thread = threading.Thread(target=handle_udp_packet, args =(sock, clients, server))
                thread.start()
            elif sock == sock_tcp:
                logging.debug("Rebut paquet TCP, creat procés per atendre'l")
                thread = threading.Thread(target=handle_tcp_packet, args=(sock, clients, server))
                thread.start()
            elif sock == sys.stdin.fileno():
                server_input = input()
                thread = threading.Thread(target=handle_commands, args=(sock, server_input, clients, server))
                thread.start()
            else:
                logging.info(f"\nUnknown socket: {sock}")
                
        thread = threading.Thread(target=check_3_alive, args=(clients,))
        thread.start()

        thread = threading.Thread(target=reset_client, args=(clients,))
        thread.start()

        if quit:
            sock_udp.close()
            logging.info("Tancat socket UDP per la comunicació amb els clients")
            sock_tcp.close()
            logging.info("Tancat socket TCP per la comunicació amb els clients")
            logging.info(f"Finalitzat procés {os.getpid()}")
            exit()


if __name__ == '__main__':
    setup()