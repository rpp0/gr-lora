#!/usr/bin/env python2
import lora, socket, pmt, time
import collections, datetime
import os.path
import xmltodict

from gnuradio import gr, gr_unittest, blocks

TestResultData    = collections.namedtuple('TestResultData', ['fromfile', 'passing', 'total', 'rate'])
TestSerieSettings = collections.namedtuple('TestSerieSettings', ['data', 'times'])

class qa_BasicTest_XML (gr_unittest.TestCase):

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

    #
    #   Compare captured data list with the expected data list.
    #   Print received data, expected data and if they match.
    #   Also give feedback about successrate in the decoding (percentage correctly captured and decoded).
    #
    def compareDataSets(self, total_data, expected_data, fromfile, cmpHDR):
        global testResults
        total_passing = 0

        print '\nReceived:'

        for idx, val in enumerate(expected_data):
            data_str = '?'
            passed = True

            try:
                data_str = " ".join("{:02x}".format(ord(n)) for n in total_data[idx])
                self.assertEqual(val, data_str if cmpHDR else data_str[9:])
            except:
                passed = False

            if passed:
                total_passing += 1
                print "{0: 3d} :: match:\n {1:s}".format(idx, val)
            else:
                print "{0: 3d} :: mismatch:\n {1:s}".format(idx, data_str)
                print "should be:\n {0:s}".format(expected_data[idx])

        results = TestResultData(fromfile, total_passing, len(expected_data), float(total_passing) / len(expected_data) * 100.0)
        testResults[len(testResults) - 1].append(results)
        print ("\nPassed rate: {0:d} out of {1:d}  ({2:.2f}%)\n"
                .format(results[1], results[2], results[3]))

    #
    #   Connect a filesource with the given file and run the flowgraph.
    #   File is acquired from the file_sink block and saved as .cfile (complex IQ data).
    #
    def runWithFileSourceAndData(self, infile, SpreadingFactor, cmpHDR):
        print "Starting test from data in: {0:s}\n".format(infile)

        if os.path.isfile(infile):
            self.file_source.close()
            self.file_source.open(infile, False)
            self.sf = SpreadingFactor
            self.lora_lora_receiver_0.set_sf(self.sf)
            self.tb.run ()

            total_data = self.gatherFromSocket(test_series[len(test_series) - 1][1])
            self.compareDataSets(total_data,
                                 test_series[len(test_series) - 1][0] * test_series[len(test_series) - 1][1],
                                 os.path.splitext(os.path.basename(infile))[0],
                                 cmpHDR)
        else:
            raise Exception("Error file does not exists!")

    #
    #   Set up flowgraph before Unit Test. (Gets threrefore called before EVERY test)
    #
    #    1. Set variables for various blocks (spreading factor from test_series settings)
    #    2. Make UDP socket server to listen for outputted messages
    #    3. Connect blocks: (file_source) -> throttle -> lora_receiver -> message_socket_sink
    #
    def setUp (self):
        ##################################################
        # Variables                                      #
        ##################################################
        self.target_freq     = 868.1e6
        self.sf              = 7
        self.samp_rate       = 1e6
        self.capture_freq    = 868.0e6
        #self.bw              = 125e3
        #self.symbols_per_sec = self.bw  / (2**self.sf)
        self.offset          = -(self.capture_freq - self.target_freq)
        #self.bitrate         = self.sf * (1 / (2**self.sf / self.bw ))

        # Socket connection for sink
        self.host            = "127.0.0.1"
        self.port            = 40868

        self.server          = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.host, self.port))
        self.server.setblocking(0)  ## or self.server.settimeout(0.5)  ?

        self.lastTestComplete = False

        self.logFile         = './examples/qa_BasicTest.log'

        self.xmlFile         = './examples/qa_BasicTest_Data.xml'

        if os.path.isfile(self.xmlFile):
            f = open(self.xmlFile, 'r')
            self.xmlTests = xmltodict.parse(f.read())["lora-test-data"]["TEST"]
            f.close()
        else:
            raise Exception("[BasicTest_XML] '" + self.xmlFile + "' does not exists!")

        self.prevData       = ""

        ##################################################
        # Blocks                                         #
        ##################################################
        self.tb = gr.top_block ()

        self.file_source = blocks.file_source(gr.sizeof_gr_complex*1, "./", False) # Repeat input: True/False
        self.lora_lora_receiver_0         = lora.lora_receiver(self.samp_rate, self.capture_freq, self.offset, self.sf, self.samp_rate)
        self.blocks_throttle_0            = blocks.throttle(gr.sizeof_gr_complex*1, self.samp_rate, True)
        self.blocks_message_socket_sink_0 = lora.message_socket_sink()

        self.tb.connect(     (self.file_source, 0),                 (self.blocks_throttle_0, 0))
        self.tb.connect(     (self.blocks_throttle_0, 0),           (self.lora_lora_receiver_0, 0))
        self.tb.msg_connect( (self.lora_lora_receiver_0, 'frames'), (self.blocks_message_socket_sink_0, 'in'))

    #
    #   Clean-up flowgraph after Unit Test. (Gets threrefore called after EVERY test)
    #
    #   If last test was completed, report results.
    #
    def tearDown (self):
        self.tb = None
        self.xmlTests = None
        self.server.close()

        if self.lastTestComplete:
            flog = open(self.logFile, 'a')
            passed_t_a = 0
            passed_t   = 0
            total_t_a  = 0
            total_t    = 0
            stro = ("-" * 8) + " Test Results on " + str(datetime.datetime.now())[:19] + " " + ("-" * 9)
            print(stro)
            flog.write(stro + '\n')

            for j, serie in enumerate(testResults):
                total_t = passed_t = 0

                stro = ("Test serie {0: 3d}: {1:s} * {2:d}"
                            .format(j, test_series[j][0], test_series[j][1]))
                print(stro)
                flog.write(stro + '\n')

                for i, x in enumerate(serie):
                    passed_t += x[1]
                    total_t  += x[2]
                    stro = ("    Test {0: 3d} :: {1:5s} {2:5s} {3:4s} {4:4s} {5:4s} :: passed {6: 3d} out of {7: 3d} ({8:6.2f}%)"
                                .format(i, *(x[0].split('_')[1:-1] + [x[1]] + [x[2]] + [x[3]])))
                    print(stro)
                    flog.write(stro + '\n')

                passed_t_a += passed_t
                total_t_a  += total_t
                stro = ("  => Total passed: {0:d} out of {1:d}  ({2:.2f}%)\n"
                            .format(passed_t, total_t, float(passed_t) / (total_t if total_t else 1.0) * 100.0))
                print(stro)
                flog.write(stro + '\n')

            stro = ("\n ====== Total passed: {0:d} out of {1:d}  ({2:.2f}%) ======"
                        .format(passed_t_a, total_t_a, float(passed_t_a) / (total_t_a if total_t_a else 1.0) * 100.0))
            print(stro)
            flog.write(stro + '\n\n')
            flog.close()

            print ("Log appended to: " + self.logFile)



###################################################################################################
#   Unit tests                                                                                    #
#       These assume a directory "./examples/lora-samples"                                        #
#       with the specified .cfiles existing.                                                      #
###################################################################################################
    # def test_032 (self):
    #     testResults.append( [] )
    #     test_series.append( TestSerieSettings(["40 0d 03 88 88 88 88"] , 10) )
    #
    #     self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_010.cfile', 6)
    #     self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_010.cfile', 7)
    #
    #     self.lastTestComplete = True

    ###############################################################################################
    #   Unit tests series from qa_BasicTest_Data.xml                                              #
    ###############################################################################################
    def test_000 (self):
        for test in self.xmlTests:
            # print (("Test {0: 3d}\n"
            #         + "  File:     {1:s}\n"
            #         + "  SF:       {2: 2d}\n"
            #         + "  All data: {3:s}\n"
            #         + "  HDR:      {4:s}\n"
            #         + "  DATA:     {5:s}\n"
            #         + "  times:    {6: 2d}")
            #         .format(int(test['@id']), test['file'], int(test['spreading-factor']),
            #                 test['expected-data-all'], test['expected-hdr'],
            #                 test['expected-data-only'], int(test['expected-times']))
            #       )

            data = test['expected-data-all']
            hasHDR = True

            if not data:
                data = test['expected-hdr']
                if not data:
                    hasHDR = False
                    data = test['expected-data-only']
                else:
                    data = data + " " + test['expected-data-only']

            infile = str(test['file'])

            if data and os.path.isfile(infile):
                if data != self.prevData:
                    testResults.append( [] )
                    test_series.append( TestSerieSettings([data], int(test['expected-times'])) )
                    self.prevData = data
                self.runWithFileSourceAndData(infile, int(test['spreading-factor']), hasHDR)
            else:
                print("No test data or file does not exist, skipping test {0: 3d}...".format(int(test['@id'])))

        self.lastTestComplete = True


if __name__ == '__main__':
    global testResults
    testResults = []
    global test_series
    test_series = []

    gr_unittest.run(qa_BasicTest_XML)
