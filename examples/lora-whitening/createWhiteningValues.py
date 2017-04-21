#!/usr/bin/python2
import collections
import os
from loranode import RN2483Controller

# from ../_examplify.py import Examplify

import os
os.sys.path.append(os.path.dirname(os.path.abspath('.')))
from _examplify import Examplify

import lora, pmt, osmosdr
from gnuradio import gr, blocks

class ReceiveWhitening:
    def __init__(self, sf = 7, output_file = './test_out.csv'):
        self.target_freq     = 868.1e6
        self.sf              = sf
        self.samp_rate       = 1e6
        self.capture_freq    = 868.0e6
        self.offset          = -(self.capture_freq - self.target_freq)

        self.inputFile       = './'
        self.outputFile      = output_file
        self.tempFile        = '/tmp/whitening_out'

        self.tb = None

    def captureSequence(self, inputFile):
        self.inputFile = inputFile

        if os.path.isfile(self.inputFile):
            self.tb = gr.top_block()

            self.file_source          = blocks.file_source(gr.sizeof_gr_complex*1, self.inputFile, False) # Repeat input: True/False
            self.lora_lora_receiver_0 = lora.lora_receiver(self.samp_rate, self.capture_freq, self.offset, self.sf, self.samp_rate)
            self.blocks_throttle_0    = blocks.throttle(gr.sizeof_gr_complex*1, self.samp_rate, True)

            self.tb.connect( (self.file_source, 0),       (self.blocks_throttle_0, 0))
            self.tb.connect( (self.blocks_throttle_0, 0), (self.lora_lora_receiver_0, 0))

            self.tb.run()

            self.tb = None

            if os.path.isfile(self.tempFile):
                if os.path.isfile(self.outputFile):
                    inf = open(self.tempFile, 'r')
                    seq = inf.read()
                    # print(seq)
                    out = open(self.outputFile, 'a')
                    out.write(seq)
                    out.close()
                    inf.close()
                else:
                    raise Exception("[ReceiveWhitening] Outputfile '" + self.outputFile + "' does not exist!")
            else:
                raise Exception("[ReceiveWhitening] Tempfile '" + self.tempFile + "' does not exist!")
        else:
            raise Exception("[ReceiveWhitening] Inputfile '" + self.inputFile + "' does not exist!")

if __name__ == '__main__':
    ofile = '/tmp/tmp_whitening.cfile'

    testset = [ (7, "4/6"), (7, "4/7"), (8, "4/5"), (12, "4/6"), (9, "4/5"), (10, "4/5"), (11, "4/5"), (6, "4/5")]

    for settings in testset:
        dataf = './test_out_SF{0:d}_CR{1:s}.csv'.format(settings[0], '-'.join(settings[1].split('/')))
        out = open(dataf, 'a')
        out.close()

        examplifr = Examplify(settings[0], settings[1], gains = [32, 38, 38])
        whitening = ReceiveWhitening(settings[0], dataf)

        for i in range(8):
            print("Sample {0:d} of 16".format(i))
            examplifr.transmitToFile(['0' * 256] * 4, ofile)
            whitening.captureSequence(ofile)
        for i in range(8):
            print("Sample {0:d} of 16".format(i + 8))
            examplifr.transmitToFile(['0' * 256] * 8, ofile)
            whitening.captureSequence(ofile)

        examplifr = None
        whitening = None
