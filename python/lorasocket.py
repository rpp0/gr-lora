import socket
import binascii

class LoRaUDPServer():
    def __init__(self, ip="127.0.0.1", port=40868, timeout=10):
        self.ip = ip
        self.port = port
        self.timeout = timeout

        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.s.bind((self.ip, self.port))
        self.s.settimeout(self.timeout)

    def __del__(self):
        self.s.close()

    def get_payloads(self, number_of_payloads):
        """
        Returns array of <number_of_payloads> hexadecimal LoRa payload datagrams received on a socket.
        """
        total_data = []
        data = ''

        for i in range(number_of_payloads):
            try:
                data = self.s.recvfrom(65535)[0]
                if data:
                    total_data.append(binascii.hexlify(data))
            except Exception as e:
                print(e)
                pass

        return total_data
