#!/usr/bin/python2

import struct
import time
import collections
from loranode import RN2483Controller

import lora, pmt, osmosdr
from gnuradio import gr, blocks

class Examplify:

    def __init__(self, outputFile = '/tmp/examplify_data.cfile'):
        ##################################################
        # Variables                                      #
        ##################################################
        self.target_freq     = 868.1e6
        self.sf              = 6        # 6 7 8 12
        self.samp_rate       = 1e6
        self.capture_freq    = 868.0e6
        self.bw              = 125e3
        #self.symbols_per_sec = self.bw  / (2**self.sf)
        self.offset          = -(self.capture_freq - self.target_freq)
        #self.bitrate         = self.sf * (1 / (2**self.sf / self.bw ))
        self.crc             = True
        self.pwr             = 1
        self.codingRate      = "4/6"    # 4/5 4/6 4/7

        self.outputFile      = outputFile
        self.pre_delay       = 0.150
        self.post_delay      = 0.350
        self.trans_delay     = 0.1

        ##################################################
        # LoRa transmitter                               #
        ##################################################
        self.lc = RN2483Controller("/dev/lora")
        self.lc.set_cr ( self.codingRate)
        self.lc.set_bw ( self.bw / 1e3)
        self.lc.set_sf ( self.sf )
        self.lc.set_crc( "on" if self.crc else "off")
        self.lc.set_pwr( self.pwr )

        ##################################################
        # Blocks                                         #
        ##################################################
        self.tb = gr.top_block ()

        self.osmosdr_source_0 = osmosdr.source( args="numchan=" + str(1) + " " + '' )
        self.osmosdr_source_0.set_sample_rate(self.samp_rate)
        self.osmosdr_source_0.set_center_freq(self.capture_freq, 0)
        self.osmosdr_source_0.set_freq_corr(0, 0)
        self.osmosdr_source_0.set_dc_offset_mode(0, 0)
        self.osmosdr_source_0.set_iq_balance_mode(0, 0)
        self.osmosdr_source_0.set_gain_mode(False, 0)
        self.osmosdr_source_0.set_gain(10, 0)
        self.osmosdr_source_0.set_if_gain(20, 0)
        self.osmosdr_source_0.set_bb_gain(20, 0)
        self.osmosdr_source_0.set_antenna('', 0)
        self.osmosdr_source_0.set_bandwidth(0, 0)

    def setPreDelay(self, delay_s):
        self.pre_delay = delay_s

    def setPostDelay(self, delay_s):
        self.post_delay = delay_s

    def setTransmitDelay(self, delay_s):
        self.trans_delay = delay_s

    def transmitRawData(self, data_list):
        print ("Transmitting...")
        time.sleep(self.pre_delay)
        
        for x in data_list:
            self.lc.send_p2p(x)
            time.sleep(self.trans_delay)
            
        time.sleep(self.post_delay)

    def transmitToCapture(self, data_list):
        self.blocks_file_sink_0 = blocks.file_sink(gr.sizeof_gr_complex*1, self.outputFile, False)
        self.blocks_file_sink_0.set_unbuffered(False)
        self.tb.connect( (self.osmosdr_source_0, 0), (self.blocks_file_sink_0, 0))

        print ("Start run")
        self.tb.start()
        self.transmitRawData(data_list)
        self.tb.lock()
        print ("Stop run")

        self.tb.disconnect( (self.osmosdr_source_0, 0), (self.blocks_file_sink_0, 0))
        self.blocks_file_sink_0 = None
        self.tb.unlock()
        self.tb.wait()

    def transmitToFile(self, data_list, output):
        prev = self.outputFile
        self.outputFile = output
        self.transmitToCapture(data_list)
        self.outputFile = prev


FileData = collections.namedtuple('FileData', ['path', 'data', 'times'])

def appendAndCaptureExample(examples_output, data, times, path, name):
    name = (name + "_cr{0:s}_bw{1:d}_sf{2:d}_crc{3:d}_pwr{4:d}_{5:03d}"
                    .format(e.codingRate.replace("/", "-"),
                            int(e.bw / 1e3),
                            e.sf,
                            1 if e.crc else 0,
                            e.pwr,
                            len(examples_output))
                 + ".cfile")

    examples_output.append( FileData( name, '["{0:s}"]'.format(data), times ) )

    e.transmitToFile([data] * times, path + name)

def printToFile(file_path, examples_output):
    f = open(file_path, 'a')
    stro = '\n' + (' ' * 20) + 'FILE' + (' ' * 23) + '|' + (' ' * 10) + 'HEX'
    f.write(stro + '\n')
    print stro

    stro = ('-' * 47) + '+' + ('-' * 27)
    f.write(stro + '\n')
    print stro

    for x in examples_output:
        stro = ' {0:45s} | {1:s} * {2:d}'.format(x[0], x[1], x[2])
        f.write(stro + '\n')
        print stro

    f.close()


if __name__ == '__main__':
    examples_output = []
    e = Examplify()

    appendAndCaptureExample(examples_output, "0123456789abcdef", 10,
                            '/home/william/lora-samples/', 'hackrf')

    appendAndCaptureExample(examples_output, "111111", 1,
                            '/home/william/lora-samples/', 'hackrf')
    appendAndCaptureExample(examples_output, "111111", 5,
                            '/home/william/lora-samples/', 'hackrf')

    appendAndCaptureExample(examples_output, "aaaaaaaa", 3,
                            '/home/william/lora-samples/', 'hackrf')

    appendAndCaptureExample(examples_output, "ffffffff", 1,
                            '/home/william/lora-samples/', 'hackrf')
    appendAndCaptureExample(examples_output, "ffffffff", 10,
                            '/home/william/lora-samples/', 'hackrf')

    appendAndCaptureExample(examples_output, "55555555", 3,
                            '/home/william/lora-samples/', 'hackrf')
    appendAndCaptureExample(examples_output, "55555555", 10,
                            '/home/william/lora-samples/', 'hackrf')

    appendAndCaptureExample(examples_output, "88888888", 1,
                            '/home/william/lora-samples/', 'hackrf')
    appendAndCaptureExample(examples_output, "88888888", 5,
                            '/home/william/lora-samples/', 'hackrf')
    appendAndCaptureExample(examples_output, "88888888", 10,
                            '/home/william/lora-samples/', 'hackrf')

    printToFile('/home/william/lora-samples/expected_results.txt', examples_output)
