#!/usr/bin/env python2
import lora, socket, pmt, time
import collections, datetime
import os.path

from gnuradio import gr, gr_unittest, blocks

TestResultData    = collections.namedtuple('TestResultData', ['fromfile', 'passing', 'total', 'rate'])
TestSerieSettings = collections.namedtuple('TestSerieSettings', ['data', 'times'])

class qa_BasicTest (gr_unittest.TestCase):

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
    def compareDataSets(self, total_data, expected_data, fromfile):
        global testResults
        total_passing = 0

        print '\nReceived:'

        for idx, val in enumerate(expected_data):
            data_str = '?'
            passed = True

            try:
                data_str = " ".join("{:02x}".format(ord(n)) for n in total_data[idx])
                self.assertEqual(val, data_str)
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
    def runWithFileSourceAndData(self, infile, SpreadingFactor):
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
                                os.path.splitext(os.path.basename(infile))[0])
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
    ###############################################################################################
    #   Unit tests series 0                                                                       #
    #        ["80 0b 01 01 23 45 67 89 ab cd ef"] * 10 with Coding Rate 4/5                       #
    ###############################################################################################
    def test_000 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["80 0b 01 01 23 45 67 89 ab cd ef"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_000.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_000.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_000.cfile', 12)

    ###############################################################################################
    #   Unit tests series 1                                                                       #
    #        ["80 0f 02 01 23 45 67 89 ab cd ef"] * 10 with Coding Rate 4/7                       #
    ###############################################################################################
    def test_001 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["80 0f 02 01 23 45 67 89 ab cd ef"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_000.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_000.cfile', 7)

    ###############################################################################################
    #   Unit tests series 2                                                                       #
    #        ["80 0? 0? 01 23 45 67 89 ab cd ef"] * 10 with Coding Rate 4/6                       #
    ###############################################################################################
    def test_002 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["80 0d 05 01 23 45 67 89 ab cd ef"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_000.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_000.cfile', 7)

    ###############################################################################################
    #   Unit tests series 3                                                                       #
    #        ["30 0b 02 11 11 11"] * 1 with Coding Rate 4/5                                       #
    ###############################################################################################
    def test_003 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["30 0b 02 11 11 11"], 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_001.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_001.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_001.cfile', 12)

    ###############################################################################################
    #   Unit tests series 4                                                                       #
    #        ["30 0f 01 11 11 11"] * 1 with Coding Rate 4/7                                       #
    ###############################################################################################
    def test_004 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["30 0f 01 11 11 11"], 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_001.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_001.cfile', 7)

    ###############################################################################################
    #   Unit tests series 5                                                                       #
    #        ["30 0d 06 11 11 11"] * 1 with Coding Rate 4/6                                       #
    ###############################################################################################
    def test_005 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["30 0d 06 11 11 11"], 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_001.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_001.cfile', 7)

    ###############################################################################################
    #   Unit tests series 6                                                                       #
    #        ["30 0b 02 11 11 11"] * 5 with Coding Rate 4/5                                       #
    ###############################################################################################
    def test_006 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["30 0b 02 11 11 11"], 5) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_002.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_002.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_002.cfile', 12)

    ###############################################################################################
    #   Unit tests series 7                                                                       #
    #        ["30 0f 01 11 11 11"] * 5 with Coding Rate 4/7                                       #
    ###############################################################################################
    def test_007 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["30 0f 01 11 11 11"], 5) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_002.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_002.cfile', 7)

    ###############################################################################################
    #   Unit tests series 8                                                                       #
    #        ["30 0d 06 11 11 11"] * 5 with Coding Rate 4/6                                       #
    ###############################################################################################
    def test_008 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["30 0d 06 11 11 11"], 5) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_002.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_002.cfile', 7)

    ###############################################################################################
    #   Unit tests series 9                                                                       #
    #        ["40 0b 07 aa aa aa aa"] * 3 with Coding Rate 4/5                                    #
    ###############################################################################################
    def test_009 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 aa aa aa aa"], 3) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_003.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_003.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_003.cfile', 12)

    ###############################################################################################
    #   Unit tests series 10                                                                      #
    #        ["40 0f 04 aa aa aa aa"] * 3 with Coding Rate 4/7                                    #
    ###############################################################################################
    def test_010 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 aa aa aa aa"], 3) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_003.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_003.cfile', 7)

    ###############################################################################################
    #   Unit tests series 11                                                                      #
    #        ["40 0d 03 aa aa aa aa"] * 3 with Coding Rate 4/6                                    #
    ###############################################################################################
    def test_011 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 aa aa aa aa"], 3) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_003.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_003.cfile', 7)

    ###############################################################################################
    #   Unit tests series 12                                                                      #
    #        ["40 0b 07 ff ff ff ff"] * 1 with Coding Rate 4/5                                    #
    ###############################################################################################
    def test_012 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 ff ff ff ff"], 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_004.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_004.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_004.cfile', 12)

    ###############################################################################################
    #   Unit tests series 13                                                                      #
    #        ["40 0f 04 ff ff ff ff"] * 1 with Coding Rate 4/7                                    #
    ###############################################################################################
    def test_013 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 ff ff ff ff"], 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_004.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_004.cfile', 7)

    ###############################################################################################
    #   Unit tests series 14                                                                      #
    #        ["40 0d 03 aa aa aa aa"] * 3 with Coding Rate 4/6                                    #
    ###############################################################################################
    def test_014 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 ff ff ff ff"], 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_004.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_004.cfile', 7)

    ###############################################################################################
    #   Unit tests series 15                                                                      #
    #        ["40 0b 07 ff ff ff ff"] * 10 with Coding Rate 4/5                                   #
    ###############################################################################################
    def test_015 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 ff ff ff ff"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_005.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_005.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_005.cfile', 12)

    ###############################################################################################
    #   Unit tests series 16                                                                      #
    #        ["40 0f 04 ff ff ff ff"] * 10 with Coding Rate 4/7                                   #
    ###############################################################################################
    def test_016 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 ff ff ff ff"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_005.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_005.cfile', 7)

    ###############################################################################################
    #   Unit tests series 17                                                                      #
    #        ["40 0d 03 ff ff ff ff"] * 10 with Coding Rate 4/6                                   #
    ###############################################################################################
    def test_017 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 ff ff ff ff"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_005.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_005.cfile', 7)

    ###############################################################################################
    #   Unit tests series 18                                                                      #
    #        ["40 0b 07 55 55 55 55"] * 3 with Coding Rate 4/5                                    #
    ###############################################################################################
    def test_018 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 55 55 55 55"], 3) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_006.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_006.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_006.cfile', 12)

    ###############################################################################################
    #   Unit tests series 19                                                                      #
    #        ["40 0f 04 55 55 55 55"]  * 3 with Coding Rate 4/7                                   #
    ###############################################################################################
    def test_019 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 55 55 55 55"] , 3) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_006.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_006.cfile', 7)

    ###############################################################################################
    #   Unit tests series 20                                                                      #
    #        ["40 0d 03 55 55 55 55"]  * 3 with Coding Rate 4/6                                   #
    ###############################################################################################
    def test_020 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 55 55 55 55"] , 3) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_006.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_006.cfile', 7)

    ###############################################################################################
    #   Unit tests series 21                                                                      #
    #        ["40 0b 07 55 55 55 55"] * 10 with Coding Rate 4/5                                   #
    ###############################################################################################
    def test_021 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 55 55 55 55"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_007.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_007.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_007.cfile', 12)

    ###############################################################################################
    #   Unit tests series 22                                                                      #
    #        ["40 0f 04 55 55 55 55"] * 10 with Coding Rate 4/7                                   #
    ###############################################################################################
    def test_022 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 55 55 55 55"] , 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_007.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_007.cfile', 7)

    ###############################################################################################
    #   Unit tests series 23                                                                      #
    #        ["40 0d 03 55 55 55 55"] * 10 with Coding Rate 4/6                                   #
    ###############################################################################################
    def test_023 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 55 55 55 55"] , 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_007.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_007.cfile', 7)

    ###############################################################################################
    #   Unit tests series 24                                                                      #
    #        ["40 0b 07 88 88 88 88"] * 1 with Coding Rate 4/5                                    #
    ###############################################################################################
    def test_024 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 88 88 88 88"], 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_008.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_008.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_008.cfile', 12)

    ###############################################################################################
    #   Unit tests series 25                                                                      #
    #        ["40 0f 04 88 88 88 88"] * 1 with Coding Rate 4/7                                    #
    ###############################################################################################
    def test_025 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 88 88 88 88"] , 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_008.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_008.cfile', 7)

    ###############################################################################################
    #   Unit tests series 26                                                                      #
    #        ["40 0d 03 88 88 88 88"] * 1 with Coding Rate 4/6                                    #
    ###############################################################################################
    def test_026 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 88 88 88 88"] , 1) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_008.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_008.cfile', 7)

    ###############################################################################################
    #   Unit tests series 27                                                                      #
    #        ["40 0b 07 88 88 88 88"] * 5 with Coding Rate 4/5                                    #
    ###############################################################################################
    def test_027 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 88 88 88 88"], 5) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_009.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_009.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_009.cfile', 12)

    ###############################################################################################
    #   Unit tests series 28                                                                      #
    #        ["40 0f 04 88 88 88 88"] * 5 with Coding Rate 4/7                                    #
    ###############################################################################################
    def test_028 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 88 88 88 88"] , 5) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_009.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_009.cfile', 7)

    ###############################################################################################
    #   Unit tests series 29                                                                      #
    #        ["40 0d 03 88 88 88 88"] * 5 with Coding Rate 4/6                                    #
    ###############################################################################################
    def test_029 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 88 88 88 88"] , 5) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_009.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_009.cfile', 7)

    ###############################################################################################
    #   Unit tests series 30                                                                      #
    #        ["40 0b 07 88 88 88 88"] * 10 with Coding Rate 4/5                                   #
    ###############################################################################################
    def test_030 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0b 07 88 88 88 88"], 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_010.cfile',   7)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_010.cfile',   8)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_010.cfile', 12)

    ###############################################################################################
    #   Unit tests series 31                                                                      #
    #        ["40 0f 04 88 88 88 88"] * 10 with Coding Rate 4/7                                   #
    ###############################################################################################
    def test_031(self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0f 04 88 88 88 88"] , 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf6_crc1_pwr1_010.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_010.cfile', 7)

    ###############################################################################################
    #   Unit tests series 32                                                                      #
    #        ["40 0d 03 88 88 88 88"] * 10 with Coding Rate 4/6                                   #
    ###############################################################################################
    def test_032 (self):
        testResults.append( [] )
        test_series.append( TestSerieSettings(["40 0d 03 88 88 88 88"] , 10) )

        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_010.cfile', 6)
        self.runWithFileSourceAndData('./examples/lora-samples/hackrf_cr4-6_bw125_sf7_crc1_pwr1_010.cfile', 7)

        self.lastTestComplete = True


if __name__ == '__main__':
    global testResults
    testResults = []
    global test_series
    test_series = []

    # Remove second argument, otherwise this would cause each test to run twice
    # (once to console and once to xml, both outputs are printed in the console though)
    gr_unittest.run(qa_BasicTest)  # , "qa_BasicTest.xml")
