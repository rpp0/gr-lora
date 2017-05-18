#!/usr/bin/env python2

import struct
import time
import collections
import os
from loranode import RN2483Controller

import lora, socket, pmt, osmosdr, random
from gnuradio import gr, blocks

TestResultData = collections.namedtuple('TestResultData', ['SF', 'CR', 'passing', 'total', 'rate'])

class ExamplifyLive:
    def __init__(self, spreadingFactor = 7, codingRate = "4/5",  gains = [10, 20, 20]):
        ##################################################
        # Variables                                      #
        ##################################################
        self.target_freq     = 868.1e6
        self.sf              = spreadingFactor  # 7 8 9 10 11 12
        self.samp_rate       = 1e6
        self.capture_freq    = 868.0e6
        self.bw              = 125e3
        #self.symbols_per_sec = self.bw  / (2**self.sf)
        self.offset          = -(self.capture_freq - self.target_freq)
        #self.bitrate         = self.sf * (1 / (2**self.sf / self.bw ))
        self.crc             = True
        self.pwr             = 1
        self.codingRate      = codingRate      # 4/5 4/6 4/7 4/8

        self.pre_delay       = 0.150
        self.post_delay      = 0.350
        self.trans_delay     = 0.250
        self.testResults     = None

        # Socket connection for sink
        self.host            = "127.0.0.1"
        self.port            = 40868

        self.server          = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.host, self.port))
        self.server.setblocking(0)

        ##################################################
        # LoRa transmitter                               #
        ##################################################
        try:
            self.lc = RN2483Controller("/dev/lora")
            self.lc.set_cr ( self.codingRate)
            self.lc.set_bw ( self.bw / 1e3)
            self.lc.set_sf ( self.sf )
            self.lc.set_crc( "on" if self.crc else "off")
            self.lc.set_pwr( self.pwr )
        except:
            raise Exception("Error initialising LoRa transmitter: RN2483Controller")

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
        self.osmosdr_source_0.set_gain(gains[0], 0)
        self.osmosdr_source_0.set_if_gain(gains[1], 0)
        self.osmosdr_source_0.set_bb_gain(gains[2], 0)
        self.osmosdr_source_0.set_antenna('', 0)
        self.osmosdr_source_0.set_bandwidth(0, 0)

        self.lora_lora_receiver_0         = lora.lora_receiver(self.samp_rate, self.capture_freq, self.offset, self.sf, self.samp_rate, 0.01)
        self.blocks_throttle_0            = blocks.throttle(gr.sizeof_gr_complex*1, self.samp_rate, True)
        self.blocks_message_socket_sink_0 = lora.message_socket_sink()

        self.tb.connect(     (self.osmosdr_source_0, 0),            (self.blocks_throttle_0, 0))
        self.tb.connect(     (self.blocks_throttle_0, 0),           (self.lora_lora_receiver_0, 0))
        self.tb.msg_connect( (self.lora_lora_receiver_0, 'frames'), (self.blocks_message_socket_sink_0, 'in'))


    def __del__(self):
        self.lc = None
        self.tb = None
        self.server.close()

    def setPreDelay(self, delay_s):
        self.pre_delay = delay_s

    def setPostDelay(self, delay_s):
        self.post_delay = delay_s

    def setTransmitDelay(self, delay_s):
        self.trans_delay = delay_s

    def getOutput(self):
        return self.testResults

    #
    #   Listen on socket for data, append to list if any and return list of captured data.
    #
    def gatherFromSocket(self, amount):
        total_data = []
        data = ''

        for i in range(amount):
            try:
                data = self.server.recv(4096)
                if data:
                    total_data.append(data)
            except:
                pass

        return total_data

    def transmitRawData(self, data_list):
        print ("Transmitting...")
        time.sleep(self.pre_delay)

        for x in data_list:
            self.lc.send_p2p(x)
            time.sleep(self.trans_delay)

        time.sleep(self.post_delay)

    def transmitToCapture(self, data_list):
        print ("Start run")
        self.tb.start()
        self.transmitRawData(data_list)
        self.tb.lock()
        self.tb.unlock()
        # self.tb.wait()
        self.tb.stop()
        print ("Stop run")

        total_data = self.gatherFromSocket(len(data_list))

        self.compareDataSets(data_list, total_data)

    def compareDataSets(self, transmitted, received):
        # self.testResults.['SF', 'CR', 'passing', 'total', 'rate']
        passing = 0
        total   = len(transmitted)

        for idx, val in enumerate(transmitted):
            passed = True

            try:
                # [6:] removes HDR
                data_str = ("".join("{:02x}".format(ord(n)) for n in received[idx]))[6:]

                # print("Test: {0:16s} == {1:16s}  ? {2:s}".format(val, data_str, "OK" if val == data_str else "FAIL"))
                passed = (val == data_str)
            except:
                passed = False

            if passed:
                passing += 1

        self.testResults = TestResultData(self.sf, self.codingRate, passing, total, float(passing) / total * 100.0)


if __name__ == '__main__':
    random.seed(None)

	# Default: [10, 20, 20]
	# For an usrp it might me necessary to increase the gain and lower the detection threshold.
	# e.g. [32, 38, 38] and t = 0.001, but be careful.
    gains = [10, 20, 20]

    ############################################################################
    # SF / CR test with TimesPerSetting packets of len(2,16) on each setting
    ############################################################################
    CodingRates      = [ "4/5", "4/6", "4/7", "4/8" ]
    SpreadingFactors = [ 7, 8, 9, 10, 11, 12 ]

    # CodingRates      = [ "4/5"]
    # SpreadingFactors = [ 7 ]

    TimesPerSetting  = 100

    # test_results     = []
    #
    # f = open('./live_example_results_SF_CR_tmp.csv', 'a')
    # f.write('SF,CR,PASSED,TOTAL,RATE\n')
    # f.close()
    #
    # for sf_i, sf in enumerate(SpreadingFactors):
    #     for cr_i, cr in enumerate(CodingRates):
    #         print "++++++++++  Starting test {0:3d} with SF: {1:2}  CR: {2:2}\n".format((sf_i+1)*(cr_i+1), sf, cr)
    #
    #         e = ExamplifyLive(sf, cr, gains)
    #
    #         # Generate array of strings between 2 and 16 in length with chars from 0x0 to 0xF, of length TimesPerSetting
    #         rdata = [ "".join("{0:1x}".format(random.randrange(0x0, 0xF)) for x in range(random.randrange(2, 17))) for i in range(TimesPerSetting) ]
    #         # Pad with '0' to even length
    #         rdata = [ x if len(x) % 2 == 0 else '0'+x for x in rdata ]
    #
    #         e.transmitToCapture(rdata)
    #         test_results.append(e.getOutput())
    #
    #         f = open('./live_example_results_SF_CR_tmp.csv', 'a')
    #
    #         res = e.getOutput()
    #         print ("[SF{0:2d}, CR{1:s}] : Passed rate: {2:d} out of {3:d}  ({4:.2f}%)"
    #                 .format(res.SF, res.CR, res.passing, res.total, res.rate))
    #         f.write('{0:d},{1:s},{2:d},{3:d},{4:.2f}\n'
    #                     .format(res.SF, res.CR, res.passing, res.total, res.rate))
    #         f.close()
    #
    #         e = None
    #
    #
    # # Report
    # f = open('./live_example_results_SF_CR.csv', 'w')
    # f.write('SF,CR,PASSED,TOTAL,RATE\n')
    #
    # for res in test_results:
    #     print ("[SF{0:2d}, CR{1:s}] : Passed rate: {2:d} out of {3:d}  ({4:.2f}%)"
    #             .format(res.SF, res.CR, res.passing, res.total, res.rate))
    #     f.write('{0:d},{1:s},{2:d},{3:d},{4:.2f}\n'
    #                 .format(res.SF, res.CR, res.passing, res.total, res.rate))
    # f.close()

    ############################################################################
    # Length test with TimesPerSetting packets of each length len(2,16) on best setting
    ############################################################################
    Ideal_SF = 7
    Ideal_CR = "4/8"
    test_results_length = []

    f = open('./live_example_results_length_tmp.csv', 'a')
    f.write('SF,CR,LENGTH,PASSED,TOTAL,RATE\n')
    f.close()

    for length in range(16, 66, 2): # range(2, 17): range(16, 34, 2):
        print "++++++++++  Starting test with length {0:3d} and SF: {1:2}  CR: {2:2}\n".format(length, Ideal_SF, Ideal_CR)

        e = ExamplifyLive(Ideal_SF, Ideal_CR, gains)

        # Generate array of strings between 2 and 16 in length with chars from 0x0 to 0xF, of length TimesPerSetting
        rdata = [ "".join("{0:1x}".format(random.randrange(0x0, 0xF)) for x in range(length)) for i in range(TimesPerSetting) ]
        # Pad with '0' to even length
        rdata = [ x if len(x) % 2 == 0 else '0'+x for x in rdata ]

        e.transmitToCapture(rdata)
        test_results_length.append( [e.getOutput(), length / 2] )

        f = open('./live_example_results_length_tmp.csv', 'a')
        res = e.getOutput()
        f.write('{0:d},{1:s},{2:d},{3:d},{4:d},{5:.2f}\n'
                    .format(res.SF, res.CR, length / 2, res.passing, res.total, res.rate))
        f.close()

        e = None


    # Report
    f = open('./live_example_results_length.csv', 'a')
    f.write('SF,CR,LENGTH,PASSED,TOTAL,RATE\n')

    for res in test_results_length:
        print ("[SF{0:2d}, CR{1:s}, len={2:2d}] : Passed rate: {3:3d} out of {4:d}  ({5:.2f}%)"
                .format(res[0].SF, res[0].CR, res[1], res[0].passing, res[0].total, res[0].rate))
        f.write('{0:d},{1:s},{2:d},{3:d},{4:d},{5:.2f}\n'
                    .format(res[0].SF, res[0].CR, res[1], res[0].passing, res[0].total, res[0].rate))
    f.close()
