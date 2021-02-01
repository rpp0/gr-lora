#!/usr/bin/env python3
# Gets a USRP capture trace from my research page and decodes it using gr-lora.
# Author: Pieter Robyns

from gnuradio import blocks
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio.eng_option import eng_option
from gnuradio.filter import firdes
from optparse import OptionParser
from lora.loraconfig import LoRaConfig
from time import sleep
import lora
import argparse
import os
import requests
import json

class lora_receive_file_nogui(gr.top_block):
    def __init__(self, sample_file, sample_rate, capture_freq, lc):
        gr.top_block.__init__(self, "Lora Receive File, No GUI")
        ##################################################
        # Variables
        ##################################################
        self.decimation = 1

        ##################################################
        # Blocks
        ##################################################
        self.message_socket_sink = lora.message_socket_sink('127.0.0.1', 40868, 1)
        self.lora_receiver = lora.lora_receiver(sample_rate, capture_freq, ([lc.freq]), lc.bw, lc.sf, lc.implicit, lc.cr_num, lc.crc, reduced_rate=False, decimation=self.decimation)
        self.blocks_throttle = blocks.throttle(gr.sizeof_gr_complex, sample_rate, True)
        self.blocks_file_source = blocks.file_source(gr.sizeof_gr_complex, sample_file, False)

        ##################################################
        # Connections
        ##################################################
        self.msg_connect((self.lora_receiver, 'frames'), (self.message_socket_sink, 'in'))
        self.connect((self.blocks_file_source, 0), (self.blocks_throttle, 0))
        self.connect((self.blocks_throttle, 0), (self.lora_receiver, 0))

def download_file(source, destination):
    print("[+] Downloading %s -> %s" % (source, destination)),
    try:
        response = requests.get(source, stream=True)

        with open(destination, 'wb') as f:
            for chunk in response.iter_content(1000*1000):
                f.write(chunk)
                print("."),
            print(".")
    except Exception as e:
        print("[-] " + str(e))
        exit(1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Gets a USRP capture trace from my research page and decodes it using gr-lora.")
    parser.add_argument('--file', type=str, default="example-trace", help='File name of the example trace.')
    parser.add_argument('--skip-download', default=False, action='store_true', help='Skip downloading the example trace.')
    args = parser.parse_args()

    data_file_path = os.path.join('./', args.file + '.sigmf-data')
    meta_file_path = os.path.join('./', args.file + '.sigmf-meta')

    if not args.skip_download and \
       not (os.path.exists(data_file_path) and os.path.exists(meta_file_path)) and \
       input("[?] Download test LoRa signal to decode? [y/N] ").lower() == "y":
        download_file("https://research.edm.uhasselt.be/probyns/lora/usrp-868.1-sf7-cr4-bw125-crc-0.sigmf-data", data_file_path)
        download_file("https://research.edm.uhasselt.be/probyns/lora/usrp-868.1-sf7-cr4-bw125-crc-0.sigmf-meta", meta_file_path)
        sleep(3)
    else:
        print("[+] Skipping download.")

    if os.path.exists(data_file_path) and os.path.exists(meta_file_path):
        metadata = json.load(open(meta_file_path, "r"))
        meta_global = metadata["global"]
        meta_capture = metadata["captures"][0]

        # Parse metadata. Not using SigMF library because of extra dependency
        sample_rate = meta_global["core:sample_rate"]
        capture_freq = meta_capture["core:frequency"]
        transmit_freq = meta_capture["lora:frequency"]
        sf = meta_capture["lora:sf"]
        cr = meta_capture["lora:cr"]
        bw = meta_capture["lora:bw"]
        prlen = meta_capture["lora:prlen"]
        crc = meta_capture["lora:crc"]
        implicit = meta_capture["lora:implicit"]
        lora_config = LoRaConfig(transmit_freq, sf, cr, bw, prlen, crc, implicit)
        print("[+] Configuration: %s" % lora_config.string_repr())

        # Decode the data!
        payload = meta_capture["test:expected"]
        times = meta_capture["test:times"]
        print("[+] Decoding. You should see a header, followed by '%s'%s %d times." % (payload, " and a CRC" if crc else "", times))
        tb = lora_receive_file_nogui(data_file_path, sample_rate, capture_freq, lora_config)
        tb.start()
        tb.wait()
        print("[+] Done")
        exit(0)
    else:
        print("[-] Example trace or metadata missing! Exiting.")
        exit(1)
