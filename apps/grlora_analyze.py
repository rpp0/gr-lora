#!/usr/bin/python3

# -----------------------------------------------------------------------------
# grlora_analyze.py
# A Python tool capable of performing a real-time analysis of signals passed
# by GNU Radio.
#
# Author: Pieter Robyns
# -----------------------------------------------------------------------------

import os
import socket
import numpy as np
import matplotlib.pyplot as plt
import struct
import argparse
from threading import Thread

# Pop num bytes from l
def fetch(l, num):
    fet = l[0:num]
    del l[0:num]
    return fet

def iphase(cpx):
    return np.unwrap(np.angle(cpx))

def ifreq(cpx):
    iphase_signal = iphase(cpx)
    return np.diff(iphase_signal)

def add_plot_complex(fig, pos, value, title='', grid=False, grid_spacing=8192):
    ax = fig.add_subplot(pos)
    ax.set_title(title)
    ax.plot(np.arange(len(value)), np.real(value), "b", np.arange(len(value)), np.imag(value), "g")
    if grid:
        ax.grid(color='r', linestyle='-', linewidth=2, markevery=grid_spacing, axis='x', which='minor', alpha=0.5)
        minor_ticks = np.arange(0, len(value), grid_spacing)
        ax.set_xticks(minor_ticks, minor=True)
    ax.set_xlim([0, len(value)])
    ax.set_xlabel("samples")
    return ax

class State:
    READ_HEADER = 0
    READ_DATA = 1

class Plotter(Thread):
    def __init__(self):
        Thread.__init__(self)
        self.setDaemon(True)
        self.socket_address = "/tmp/gr_lora.sock"
        self.socket = None
        self.init_socket()
        plt.ion()

    def init_socket(self):
        if self.socket is None:
            try:
                os.unlink(self.socket_address)
            except OSError:
                if os.path.exists(self.socket_address):
                    raise FileExistsError

            self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.socket.bind(self.socket_address)
            self.socket.listen(1)
            self.socket.setblocking(0)
            print("[gr-lora analyzer]: listening at " + self.socket_address)
        else:
            return # Socket is already initialized

    def get_data(self, connection):
        buffer = connection.recv(1024)
        if buffer:
            return buffer
        else:
            raise ConnectionResetError

    def run(self):
        while True:
            plt.pause(0.0001)  # Give some time to Qt GUI to render
            try:
                client_socket, client_address = self.socket.accept()
                try:
                    self.state = State.READ_HEADER
                    data = bytearray()
                    data_len = 0
                    draw_over = False
                    fig = plt.figure()

                    while True:
                        plt.pause(0.0001)
                        try:
                            # Parse buffer data
                            if self.state == State.READ_HEADER:
                                while len(data) < 5:
                                    data += self.get_data(client_socket)

                                data_len, draw_over = struct.unpack(">I?", fetch(data, 5))
                                self.state = State.READ_DATA
                            elif self.state == State.READ_DATA:
                                while len(data) < data_len:
                                    data += self.get_data(client_socket)

                                plot_data = np.frombuffer(fetch(data, data_len), dtype=np.complex64)
                                if not draw_over:
                                    plt.gcf().clear()
                                add_plot_complex(fig, 211, plot_data, grid=True)
                                add_plot_complex(fig, 212, ifreq(plot_data), grid=True)
                                #plt.plot(np.arange(len(plot_data)), np.real(plot_data), "b", np.arange(len(plot_data)), np.imag(plot_data), "g")
                                self.state = State.READ_HEADER
                        except ConnectionResetError:
                            print("[gr-lora analyzer]: connection reset")
                            break
                finally:
                    # Clean up the connection
                    client_socket.close()
            except BlockingIOError:
                pass

# Test operation of the Plotter class
def test():
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect("/tmp/gr_lora.sock")
    try:
        print("[fake client]: sending!")
        with open(os.path.expanduser("~/usrpsf7.cfile"),"rb") as f:
            data = f.read()
            chunk_size = 524288
            for i in range(0, len(data), chunk_size):
                chunk = data[i:i+chunk_size]
                data_len = len(chunk)
                draw_over = False
                client.sendall(struct.pack(">I?", data_len, draw_over) + chunk)
    finally:
        print("[fake client]: done sending!")
        client.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='gr-lora debugging / analysis tool for interfacing with GNU Radio over Unix sockets')
    parser.add_argument('--test', dest='test', help='Peform a test', action='store_true')
    args, unknown = parser.parse_known_args()
    plotter = Plotter()
    plotter.start()

    if args.test:
        test()

    plotter.join()
    exit(0)
