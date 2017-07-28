#!/usr/bin/env python2
# coding=utf8

import collections
import os.path
import re

from gnuradio import gr, blocks
from gnuradio import uhd
import osmosdr
import lora

LoRaReceiver = collections.namedtuple('LoRaReceiver', ['name', 'available'])

class LoRaReceiveAll:
    def __init__(self, receiver, spreadingFactor = 7, samp_rate = 1e6, capture_freq = 868.0e6, target_freq = 868.1e6, center_offset = 0, threshold = 0.01):
        ##################################################
        # Variables                                      #
        ##################################################
        self.target_freq     = target_freq
        self.sf              = spreadingFactor  # 6 7 8 12
        self.samp_rate       = samp_rate
        self.capture_freq    = capture_freq
        self.bw              = 125e3
        #self.symbols_per_sec = self.bw  / (2**self.sf)
        self.offset          = -(self.capture_freq - self.target_freq)
        #self.bitrate         = self.sf * (1 / (2**self.sf / self.bw ))
        # self.crc             = True
        # self.pwr             = 1
        # self.codingRate      = codingRate      # 4/5 4/6 4/7
        self.threshold       = threshold

        # For FFT, determine Center Frequency Offset first, then set here.
        # For RN2483, usually -14.1e3
        self.center_offset   = center_offset

        ##################################################
        # Blocks                                         #
        ##################################################
        self.tb = gr.top_block ()

        self.source                    = receiver
        self.lora_lora_receiver_0      = lora.lora_receiver(self.samp_rate, self.capture_freq, ([868.1e6]), self.sf, self.samp_rate, self.threshold)
        self.blocks_throttle_0         = blocks.throttle(gr.sizeof_gr_complex*1, self.samp_rate, True)
        self.freq_xlating_fir_filter_0 = filter.freq_xlating_fir_filter_ccc(1, (firdes.low_pass(1, self.samp_rate, 200000, 50000)), self.center_offset, self.samp_rate)

        self.tb.connect( (self.source, 0),                    (self.blocks_throttle_0, 0))
        self.tb.connect( (self.blocks_throttle_0, 0),         (self.freq_xlating_fir_filter_0, 0))
        self.tb.connect( (self.freq_xlating_fir_filter_0, 0), (self.lora_lora_receiver_0, 0))

    def start(self):
        # self.tb.Start(True)
        # self.tb.Wait()
        self.tb.run()
        self.tb = None


if __name__ == '__main__':
    receivers = [ LoRaReceiver("OsmoSDR/RTL-SDR/HackRF", True),
                  LoRaReceiver("NI USRP"               , True),
                  LoRaReceiver("Filesource"            , True) ]

    print("Configure settings:")
    while True:
        sf = int(input("  SF: "))
        if sf in range(6, 13):
            break

    target_freq   = 868.1e6
    samp_rate     = 1e6
    capture_freq  = 868.0e6
    center_offset = 0

    print("Available sources:")
    for i, r in enumerate(receivers):
        if r.available:
            print("  {0: 2d}: {1:s}".format(i, r.name))

    while True:
        choice = int(input("Please enter the number for your desired receiver: "))
        if (choice in range(0, len(receivers))) and receivers[choice].available:
            break

    receiver = None

    if choice == 0:
        # Osmo SDR, RTL-SDR, HackRF
        receiver = osmosdr.source( args="numchan=" + str(1) + " " + '' )
        receiver.set_sample_rate(samp_rate)
        receiver.set_center_freq(capture_freq, 0)
        receiver.set_freq_corr(0, 0)
        receiver.set_dc_offset_mode(0, 0)
        receiver.set_iq_balance_mode(0, 0)
        receiver.set_gain_mode(False, 0)
        receiver.set_gain(10, 0)
        receiver.set_if_gain(20, 0)
        receiver.set_bb_gain(20, 0)
        receiver.set_antenna('', 0)
        receiver.set_bandwidth(0, 0)
    elif choice == 1:
        # USRP
        address = ""
        # while True:
        #     address = input("Please specify the USRP's IP address: ")
        #     if re.fullmatch(ur"^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$", address) is not None:
        #         break

        receiver = uhd.usrp_source(
        	",".join((address, "")),
        	uhd.stream_args(
        		cpu_format="fc32",
        		channels=range(1),
        	),
        )
        receiver.set_samp_rate(samp_rate)
        receiver.set_center_freq(capture_freq, 0)
        receiver.set_gain(32, 0)
        receiver.set_antenna('RX2', 0)
    elif choice == 2:
        # File
        while True:
            inputFile = raw_input("Please specify an input file with IQ data: ")
            if len(inputFile) > 0 and os.path.isfile(inputFile):
                break
        receiver = blocks.file_source(gr.sizeof_gr_complex*1, inputFile, False) # Repeat input: True/False

    if receiver is None:
        print("Warning: No receiver set!")
        exit()

    sdr = LoRaReceiveAll(receiver, sf, samp_rate, capture_freq, target_freq, center_offset)
    sdr.start()
