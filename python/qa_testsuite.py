#!/usr/bin/env python2
import lora
import socket
import pmt
import time
import collections
import datetime
import os.path
import os
import json
import binascii
import signal
import argparse

from gnuradio import gr, gr_unittest, blocks, filter
from gnuradio.filter import firdes
from sigmf.sigmffile import SigMFFile
from lora.loraconfig import LoRaConfig

Test = collections.namedtuple('Test', ['payload', 'times'])
TestResult = collections.namedtuple('TestResult', ['decoded_data', 'lora_config', 'test'])

def signal_handler(signal, frame):
        exit(0)

def trunc(target, max_len=30):
    result = ""
    if len(target) > max_len:
        result += target[0:max_len/2-1]
        result += ".."
        result += target[-max_len/2+1:]
    else:
        result = target
    assert(len(result) <= max_len)
    return result

class TestSummary():
    def __init__(self, suite):
        self.suite = suite
        self._summary = []
        self._summary_text = "-------- Test suite '{:s}' results on {:s} ---------\n".format(suite, str(datetime.datetime.utcnow()))
        self._summary_markdown = ""
        self._num_total_correct_payloads = 0
        self._num_total_payloads = 0
        self._num_tests = 0
        self._last_config = None

    def add(self, test_result, print_intermediate=False):
        if type(test_result) == TestResult:
            self._summary.append(test_result)
            self._evaluate_result(test_result, print_intermediate)
        else:
            raise Exception("Test result must be of type TestResult")

    def export_summary(self, print_output=True):
        self._summary_text += "\nRan a total of {:n} tests, together containing {:n} payloads.\n".format(
            self._num_tests,
            self._num_total_payloads
        )
        self._summary_text += "====== Total payloads passed: {:>5n} out of {:<5n}  ({:.2%}) ======\n".format(
            self._num_total_correct_payloads,
            self._num_total_payloads,
            float(self._num_total_correct_payloads) / self._num_total_payloads
        )

        if print_output:
            print(self._summary_text)

    def _evaluate_result(self, test_result, print_intermediate):
        """
        Given a test result, evaluate it and generate text / markdown for the report.
        """
        self._num_tests += 1
        evaluation_text = ""
        evaluation_markdown = ""

        # Shorter names
        decoded_data = test_result.decoded_data
        lora_config = test_result.lora_config
        test = test_result.test
        expected_data = [test.payload] * test.times

        # Don't reprint configuration if it is the same as before
        if(self._last_config != vars(lora_config)):
            evaluation_text += "Configuration {:s}:\n".format(lora_config.string_repr())
            self._last_config = vars(lora_config)

        # Determine number of correct payloads
        num_payloads = 0
        num_correct_payloads = 0
        for i in range(0, test.times):
            num_payloads += 1
            self._num_total_payloads += 1

            try:
                decoded = decoded_data[i]
            except IndexError:
                decoded = "?"
            try:
                expected = expected_data[i]
            except IndexError:
                expected = "?"

            if decoded == expected:
                num_correct_payloads += 1
                self._num_total_correct_payloads += 1

        # Append to text report
        evaluation_text += "\tTest {:>3n}: {:<30s} * {:<3n} :: passed {:>3n} out of {:<3n} ({:.2%})\n".format(self._num_tests, trunc(test.payload), test.times, num_correct_payloads, num_payloads, float(num_correct_payloads)/num_payloads)
        self._summary_text += evaluation_text

        if(print_intermediate):
            print(evaluation_text)


class qa_testsuite():
    def __init__(self):
        """
        Determine installed test suites and setup socket server for receiving payloads decoded by gr-lora.
        """
        # Variables
        self.center_offset = 0
        self.host = "127.0.0.1"
        self.port = 40868

        # Setup socket
        self.server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.host, self.port))
        self.server.settimeout(10)

        # Determine test suites directory
        current_dir = os.path.dirname(os.path.realpath(__file__)) + "/"
        self.test_suites_directory = os.path.abspath(current_dir + '../apps/test-suites')

        # List test suites
        self.test_suites = []
        if os.path.exists(self.test_suites_directory):
            self.test_suites = [x for x in os.listdir(self.test_suites_directory) if os.path.isdir(os.path.join(self.test_suites_directory, x))]
        else:
            print("No test suites found! Skipping...")

    def __del__(self):
        self.server.close()

    def get_payloads(self, number):
        """
        Returns array of <number> hexadecimal LoRa payload datagrams received on a socket.
        """
        total_data = []
        data = ''

        for i in range(number):
            try:
                data = self.server.recvfrom(65535)[0]
                if data:
                    total_data.append(binascii.hexlify(data[3:]))  # Discard header and convert to hex text
            except Exception as e:
                print(e)
                pass

        return total_data

    def run(self, suites_to_run):
        for test_suite in self.test_suites:
            # Skip test suites that we don't want to run
            if suites_to_run != [] and (not test_suite in suites_to_run):
                continue

            print("[+] Testing suite: '%s'" % test_suite)
            summary = TestSummary(suite=test_suite)

            # Get all metadata files associated with the suite
            get_mtime = lambda f: os.stat(os.path.join(self.test_suites_directory, test_suite, f)).st_mtime
            metadata_files = [os.path.join(self.test_suites_directory, test_suite, x) for x in sorted(os.listdir(os.path.join(self.test_suites_directory, test_suite)), key=get_mtime) if x.endswith('.sigmf-meta')]

            # Parse metadata files
            for metadata_file in metadata_files:
                print("[+] %s" % metadata_file)
                data_file = os.path.splitext(metadata_file)[0] + '.sigmf-data'
                # Load sigmf data TODO abstract
                f = open(metadata_file, 'r')
                sigmf = SigMFFile(metadata=f.read())
                if not sigmf.validate():
                    raise Exception("Invalid SigMF format")
                global_meta = sigmf.get_global_info()
                capture_meta = sigmf.get_capture_info(0)
                f.close()

                # Initialize test parameters
                sample_rate = global_meta["core:sample_rate"]

                # Get LoRa configuration
                capture_freq = capture_meta["core:frequency"]
                transmit_freq = capture_meta["lora:frequency"]
                sf = capture_meta["lora:sf"]
                cr = capture_meta["lora:cr"]
                bw = capture_meta["lora:bw"]
                prlen = capture_meta["lora:prlen"]
                crc = capture_meta["lora:crc"]
                implicit = capture_meta["lora:implicit"]
                lora_config = LoRaConfig(transmit_freq, sf, cr, bw, prlen, crc, implicit)

                # Get test case configuration
                payload = capture_meta["test:expected"]
                times = capture_meta["test:times"]
                test = Test(payload, times)

                # Build flowgraph
                tb = gr.top_block()
                file_source = blocks.file_source(gr.sizeof_gr_complex, data_file, False)
                lora_receiver = lora.lora_receiver(sample_rate, capture_freq, [868100000], sf, 1000000, 0.002)
                throttle = blocks.throttle(gr.sizeof_gr_complex, sample_rate, True)
                message_socket_sink = lora.message_socket_sink()
                freq_xlating_fir_filter = filter.freq_xlating_fir_filter_ccc(1, (firdes.low_pass(1, sample_rate, 200000, 100000, firdes.WIN_HAMMING, 6.67)), self.center_offset, sample_rate)

                # Make connections
                tb.connect((file_source, 0), (throttle, 0))
                tb.connect((throttle, 0), (freq_xlating_fir_filter, 0))
                tb.connect((freq_xlating_fir_filter, 0), (lora_receiver, 0))
                tb.msg_connect((lora_receiver, 'frames'), (message_socket_sink, 'in'))
                tb.start()
                tb.wait()

                decoded_data = self.get_payloads(times)  # Output from the flowgraph
                summary.add(TestResult(decoded_data=decoded_data, lora_config=lora_config, test=test), print_intermediate=True)
            # Finally, export the result for the suite
            summary.export_summary()

if __name__ == '__main__':
    """
    Tool to evaluate decoding test suites in apps/test-suites/
    """
    # Parse args
    parser = argparse.ArgumentParser(description="Tool to evaluate decoding test suites for gr-lora.")
    parser.add_argument('suites', type=str, nargs="*", help='Names of the test suites to execute.')
    args = parser.parse_args()

    # Make sure CTRL+C exits the whole test suite instead of only the current GNU Radio top block
    signal.signal(signal.SIGINT, signal_handler)

    suite = qa_testsuite()
    suite.run(args.suites)
