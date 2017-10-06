#!/usr/bin/python2
# A tool to generate test suites for gr-lora
# Authors: Pieter Robyns and William Thenaers

import time
import collections
import os
import argparse
import copy
import lora
import pmt
import osmosdr
from loranode import RN2483Controller
from datetime import datetime
from sigmf.sigmffile import SigMFFile
from gnuradio import gr, blocks
from lora.loraconfig import LoRaConfig

Test = collections.namedtuple('Test', ['payload', 'times'])

class TestSuite():
    def __init__(self, lc, name, args, config_set, test_set):
        self.name = name
        self.config_set = config_set
        self.test_set = test_set
        self.capture_freq = args.frequency
        self.frequency_offset = args.frequency_offset
        self.sample_rate = args.sample_rate
        self.hw = args.hw
        self.name += "_" + self.hw
        self.path = os.path.join(args.data_out, self.name)
        self.pre_delay = 0.150
        self.post_delay = 1.0
        self.intra_delay = 0.1
        self.lc = lc
        self.test_count = 0

        # Prepare SigMF global metadata (identical for all tests)
        self.global_meta = {
            "core:datatype": "cf32_le",
            "core:version": "0.0.1",
            "core:license": "CC0",
            "core:hw": self.hw,
            "core:sample_rate": self.sample_rate,
            "core:author": "Pieter Robyns"
        }

        # Prepare paths for storing suite
        try:
            if not os.path.exists(self.path):
                os.makedirs(self.path)
        except:
            Exception("Error creating output directory.")

    def __del__(self):
        self.lc = None

    def run(self):
        for config in self.config_set:
            for test in self.test_set:
                self._run_test(config, test)

    def _run_test(self, config, test):
        test_name = "{:s}-{:s}-{:n}".format(self.hw, config.file_repr(), self.test_count)
        test_data_path = os.path.join(self.path, test_name + '.sigmf-data')
        test_meta_path = os.path.join(self.path, test_name + '.sigmf-meta')
        self.test_count += 1
        capture_meta = {
            "core:sample_start": 0,
            "core:frequency": self.capture_freq,
            "core:datetime": str(datetime.utcnow()),
            "lora:frequency": config.freq,
            "lora:frequency_offset": self.frequency_offset,
            "lora:sf": config.sf,
            "lora:cr": config.cr,
            "lora:bw": config.bw,
            "lora:prlen": config.prlen,
            "lora:crc": config.crc,
            "lora:implicit": config.implicit,
            "test:expected": test.payload,
            "test:times": test.times,
        }

        # Configure transmitter
        try:
            #self.lc.set_freq(config.freq)
            self.lc.set_sf(config.sf)
            self.lc.set_cr(config.cr)
            self.lc.set_bw(config.bw / 1e3)
            self.lc.set_prlen(str(config.prlen))
            self.lc.set_crc("on" if config.crc else "off")
            #self.lc.set_implicit("on" if config.implicit else "off")
            self.lc.set_pwr(1)
        except Exception as e:
            print(e)
            exit(1)

        # Build GNU Radio flowgraph
        gr.enable_realtime_scheduling()
        tb = gr.top_block()
        osmosdr_source = osmosdr.source(args="numchan=" + str(1) + " " + '' )
        osmosdr_source.set_sample_rate(self.sample_rate)
        osmosdr_source.set_center_freq(self.capture_freq, 0)
        osmosdr_source.set_freq_corr(0, 0)
        osmosdr_source.set_dc_offset_mode(0, 0)
        osmosdr_source.set_iq_balance_mode(0, 0)
        osmosdr_source.set_gain_mode(False, 0)
        osmosdr_source.set_gain(10, 0)
        osmosdr_source.set_if_gain(20, 0)
        osmosdr_source.set_bb_gain(20, 0)
        osmosdr_source.set_antenna('', 0)
        osmosdr_source.set_bandwidth(0, 0)

        file_sink = blocks.file_sink(gr.sizeof_gr_complex, test_data_path, False)

        # Connect blocks
        tb.connect((osmosdr_source, 0), (file_sink, 0))

        # Run
        print("Running %s" % test_name)
        tb.start()
        self.transmit_data(test)
        tb.stop()
        tb.wait()

        # Save metadata file
        with open(test_meta_path, 'w') as f:
            test_sigmf = SigMFFile(data_file=test_data_path, global_info=copy.deepcopy(self.global_meta))
            test_sigmf.add_capture(0, metadata=capture_meta)
            test_sigmf.dump(f, pretty=True)

    def transmit_data(self, test):
        time.sleep(self.pre_delay)

        for i in range(0, test.times):
            self.lc.send_p2p(test.payload)
            time.sleep(self.intra_delay)

        time.sleep(self.post_delay)

if __name__ == '__main__':
    """
    Generate test suites.
    """
    parser = argparse.ArgumentParser(description="Tool to generate test suites for gr-lora.")
    parser.add_argument('hw', type=str, help='SDR used to capture test suite.')
    parser.add_argument('-O', '--data-out', type=str, default='./test-suites/', help='Output directory for the test suites.')
    parser.add_argument('-s', '--sample-rate', type=int, default=1000000, help='Sample rate of the SDR.')
    parser.add_argument('-f', '--frequency', type=int, default=868e6, help='Center frequency.')
    parser.add_argument('-F', '--frequency-offset', type=int, default=0, help='Frequency offset.')
    args = parser.parse_args()

    # ------------------------------------------------------------------------
    # Test suite: decode_long
    # A quick test suite for long payloads
    # ------------------------------------------------------------------------
    decode_long_config_set = [LoRaConfig(freq=868.1e6, sf=7, cr="4/8"),
                              LoRaConfig(freq=868.1e6, sf=8, cr="4/8"),
                              LoRaConfig(freq=868.1e6, sf=9, cr="4/8"),
                              LoRaConfig(freq=868.1e6, sf=10, cr="4/8"),
                              LoRaConfig(freq=868.1e6, sf=11, cr="4/8"),
                              LoRaConfig(freq=868.1e6, sf=12, cr="4/8"),
                             ]

    decode_long_test_set = [Test(payload="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfe", times=1),]
    TestSuite(lc=RN2483Controller("/dev/lora"), name='decode_long', args=args, config_set=decode_long_config_set, test_set=decode_long_test_set).run()

    # ------------------------------------------------------------------------
    # Test suite: short
    # A test suite with several short payloads for all configurations. Inten-
    # ded for all-around testing of syncing and decoding.
    # ------------------------------------------------------------------------
    short_config_set = [LoRaConfig(freq=868.1e6, sf=7, cr="4/8"),
                        LoRaConfig(freq=868.1e6, sf=7, cr="4/7"),
                        LoRaConfig(freq=868.1e6, sf=7, cr="4/6"),
                        LoRaConfig(freq=868.1e6, sf=7, cr="4/5"),
                        LoRaConfig(freq=868.1e6, sf=8, cr="4/8"),
                        LoRaConfig(freq=868.1e6, sf=8, cr="4/7"),
                        LoRaConfig(freq=868.1e6, sf=8, cr="4/6"),
                        LoRaConfig(freq=868.1e6, sf=8, cr="4/5"),
                        LoRaConfig(freq=868.1e6, sf=9, cr="4/8"),
                        LoRaConfig(freq=868.1e6, sf=9, cr="4/7"),
                        LoRaConfig(freq=868.1e6, sf=9, cr="4/6"),
                        LoRaConfig(freq=868.1e6, sf=9, cr="4/5"),
                        LoRaConfig(freq=868.1e6, sf=10, cr="4/8"),
                        LoRaConfig(freq=868.1e6, sf=10, cr="4/7"),
                        LoRaConfig(freq=868.1e6, sf=10, cr="4/6"),
                        LoRaConfig(freq=868.1e6, sf=10, cr="4/5"),
                        LoRaConfig(freq=868.1e6, sf=11, cr="4/8"),
                        LoRaConfig(freq=868.1e6, sf=11, cr="4/7"),
                        LoRaConfig(freq=868.1e6, sf=11, cr="4/6"),
                        LoRaConfig(freq=868.1e6, sf=11, cr="4/5"),
                        LoRaConfig(freq=868.1e6, sf=12, cr="4/8"),
                        LoRaConfig(freq=868.1e6, sf=12, cr="4/7"),
                        LoRaConfig(freq=868.1e6, sf=12, cr="4/6"),
                        LoRaConfig(freq=868.1e6, sf=12, cr="4/5"),
                       ]

    short_test_set = [Test(payload="deadbeef", times=5),
                      Test(payload="88", times=1),
                      Test(payload="ffff", times=10),
                     ]
    TestSuite(lc=RN2483Controller("/dev/lora"), name='short_rn', args=args, config_set=short_config_set, test_set=short_test_set).run()
