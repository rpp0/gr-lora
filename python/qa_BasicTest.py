import lora, socket, pmt, time
import collections

from gnuradio import gr, gr_unittest, blocks
from gnuradio.blocks import socket_pdu

TestResultData    = collections.namedtuple('TestResultData', ['passing', 'total', 'rate'])
TestSerieSettings = collections.namedtuple('TestSerieSettings', ['description', 'SpreadingFactor'])

class qa_BasicTest (gr_unittest.TestCase):

    #
    #   Listen on socket for data, append to list if any and return list of captured data.
    #
    def gatherFromSocket(self):
        timeout = 0.5
        total_data = []
        data = ''
        begin = time.time()
        while 1:
            if total_data and time.time()-begin > timeout:
                break
            elif time.time()-begin > timeout*2:
                break
            try:
                data = self.server.recv(8192)
                if data:
                    total_data.append(data)
                    begin = time.time()
                else:
                    time.sleep(0.1)
            except:
                pass

        return total_data

    #
    #   Compare captured data list with the expected data list.
    #   Print received data, expected data and if they match.
    #   Also give feedback about successrate in the decoding (percentage correctly captured and decoded).
    #
    def compareDataSets(self, total_data, expected_data):
        total_passing = 0.0

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

        results = TestResultData(total_passing, len(expected_data), float(total_passing) / len(expected_data) * 100.0)
        testResults[nextSeriesSetting].append(results)
        print ("\nPassed rate: {0:d} out of {1:d}  ({2:.2f}%)\n"
                .format(results[0], results[1], results[2]))

    #
    #   Connect a filesource with the given file and run the flowgraph.
    #   File is acquired from the file_sink block and saved as .cfile (complex IQ data).
    #
    def runWithFileSourceAndData(self, file, expected_data):
        print "Starting test from data in: %s\n" % (file)

        file_source = blocks.file_source(gr.sizeof_gr_complex*1, file, False) # Repeat input: True/False
        self.tb.connect((file_source, 0), (self.blocks_throttle_0, 0))
        self.tb.run ()
        self.tb.disconnect((file_source, 0), (self.blocks_throttle_0, 0))

        total_data = self.gatherFromSocket()
        self.compareDataSets(total_data, expected_data)

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
        self.sf              = test_series[nextSeriesSetting][1] # 7
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
        self.server.setblocking(0)

        self.lastTestComplete = False

        ##################################################
        # Blocks                                         #
        ##################################################
        self.tb = gr.top_block ()

        self.lora_lora_receiver_0         = lora.lora_receiver(self.samp_rate, self.capture_freq, self.offset, self.sf, self.samp_rate)
        self.blocks_throttle_0            = blocks.throttle(gr.sizeof_gr_complex*1, self.samp_rate, True)
        self.blocks_message_socket_sink_0 = lora.message_socket_sink()

        self.tb.connect(     (self.blocks_throttle_0, 0),            (self.lora_lora_receiver_0, 0))
        self.tb.msg_connect( (self.lora_lora_receiver_0, 'frames'),  (self.blocks_message_socket_sink_0, 'in'))

    #
    #   Clean-up flowgraph after Unit Test. (Gets threrefore called after EVERY test)
    #
    #   If last test was completed, report results.
    #
    def tearDown (self):
        self.tb = None
        self.server.close()

        if self.lastTestComplete:
            passed_t_a = 0
            passed_t   = 0
            total_t_a  = 0
            total_t    = 0
            print ("-" * 18) + "  Test Results  " + ("-" * 19)

            for j, serie in enumerate(testResults):
                total_t = passed_t = 0
                
                print ("Test serie {0: 3d}: {1:5s} {2:5s} {3:5s} {4:5s} {5:5s}"
                            .format(j, *test_series[j][0].split("_")))
                            
                for i, x in enumerate(serie):
                    passed_t += x[0]
                    total_t  += x[1]
                    print ("    Test {0: 4d} passing rate: {1: 3d} out of {2: 3d}  ({3:6.2f}%)"
                                .format(i, x[0], x[1], x[2]))
                                
                passed_t_a += passed_t
                total_t_a  += total_t
                print ("  => Total passed: {0:d} out of {1:d}  ({2:.2f}%)\n"
                            .format(passed_t, total_t, float(passed_t) / total_t * 100.0))

            print ("\n ====== Total passed: {0:d} out of {1:d}  ({2:.2f}%) ======"
                        .format(passed_t_a, total_t_a, float(passed_t_a) / total_t_a * 100.0))
    


    ###############################################################################################
    #   Unit tests                                                                                #
    #       These assume a directory "lora-samples" with the specified .cfiles existing.          #
    ###############################################################################################

    ###############################################################################################
    #   Unit tests series 0                                                                       #
    #        cr4-5_bw125_sf7_crc1_pwr1                                                            #
    ###############################################################################################
    #
    #   Unit test 1
    #
    def test_001 (self):
        expected_data = [
            '71 1b 09 12 12 12 12 12 12 12 10 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12',
            '71 1b 09 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88',
            '71 1b 09 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22'
        ]

        self.runWithFileSourceAndData('/home/william/lora-samples/usrpcr5.cfile', expected_data)
        nextSeriesSetting = 1
        testResults.append([])


    ###############################################################################################
    #   Unit tests series 1                                                                       #
    #        cr4-5_bw125_sf7_crc1_pwr1                                                            #
    ###############################################################################################
    #
    #   Unit test 2
    #
    def test_002 (self):
        expected_data = ["80 0b 01 01 23 45 67 89 ab cd ef"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_000.cfile', expected_data)

    #
    #   Unit test 3
    #
    def test_003 (self):
        expected_data = ["30 0b 02 11 11 11"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_001.cfile', expected_data)

    #
    #   Unit test 4
    #
    def test_004 (self):
        expected_data = ["30 0b 02 11 11 11"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_002.cfile', expected_data)

    #
    #   Unit test 5
    #
    def test_005 (self):
        expected_data = ["40 0b 07 aa aa aa aa"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_003.cfile', expected_data)

    #
    #   Unit test 6
    #
    def test_006 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_004.cfile', expected_data)

    #
    #   Unit test 7
    #
    def test_007 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_005.cfile', expected_data)

    #
    #   Unit test 8
    #
    def test_008 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_006.cfile', expected_data)

    #
    #   Unit test 9
    #
    def test_009 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_007.cfile', expected_data)

    #
    #   Unit test 10
    #
    def test_010 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_008.cfile', expected_data)

    #
    #   Unit test 11
    #
    def test_011 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_009.cfile', expected_data)

    #
    #   Unit test 12
    #
    def test_012 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_010.cfile', expected_data)
        nextSeriesSetting = 2
        testResults.append([])

    ###############################################################################################
    #   Unit tests series 2                                                                       #
    #        cr4-5_bw125_sf12_crc1_pwr1                                                           #
    ###############################################################################################
    #
    #   Unit test 13
    #
    def test_013 (self):
        expected_data = ["80 0b 01 01 23 45 67 89 ab cd ef"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_000.cfile', expected_data)

    #
    #   Unit test 14
    #
    def test_014 (self):
        expected_data = ["30 0b 02 11 11 11"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_001.cfile', expected_data)

    #
    #   Unit test 15
    #
    def test_015 (self):
        expected_data = ["30 0b 02 11 11 11"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_002.cfile', expected_data)

    #
    #   Unit test 16
    #
    def test_016 (self):
        expected_data = ["40 0b 07 aa aa aa aa"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_003.cfile', expected_data)

    #
    #   Unit test 17
    #
    def test_017 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_004.cfile', expected_data)

    #
    #   Unit test 18
    #
    def test_018 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_005.cfile', expected_data)

    #
    #   Unit test 19
    #
    def test_019 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_006.cfile', expected_data)

    #
    #   Unit test 20
    #
    def test_020 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_007.cfile', expected_data)

    #
    #   Unit test 21
    #
    def test_021 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_008.cfile', expected_data)

    #
    #   Unit test 22
    #
    def test_022 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_009.cfile', expected_data)

    #
    #   Unit test 23
    #
    def test_023 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf12_crc1_pwr1_010.cfile', expected_data)
        nextSeriesSetting = 3
        testResults.append([])

    ###############################################################################################
    #   Unit tests series 3                                                                       #
    #        cr4-5_bw125_sf8_crc1_pwr1                                                            #
    ###############################################################################################
    #
    #   Unit test 24
    #
    def test_024 (self):
        expected_data = ["80 0b 01 01 23 45 67 89 ab cd ef"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_000.cfile', expected_data)

    #
    #   Unit test 25
    #
    def test_025 (self):
        expected_data = ["30 0b 02 11 11 11"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_001.cfile', expected_data)

    #
    #   Unit test 26
    #
    def test_026 (self):
        expected_data = ["30 0b 02 11 11 11"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_002.cfile', expected_data)

    #
    #   Unit test 27
    #
    def test_027 (self):
        expected_data = ["40 0b 07 aa aa aa aa"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_003.cfile', expected_data)

    #
    #   Unit test 28
    #
    def test_028 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_004.cfile', expected_data)

    #
    #   Unit test 29
    #
    def test_029 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_005.cfile', expected_data)

    #
    #   Unit test 30
    #
    def test_030 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_006.cfile', expected_data)

    #
    #   Unit test 31
    #
    def test_031 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_007.cfile', expected_data)

    #
    #   Unit test 32
    #
    def test_032 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_008.cfile', expected_data)

    #
    #   Unit test 33
    #
    def test_033 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_009.cfile', expected_data)

    #
    #   Unit test 34
    #
    def test_034 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf8_crc1_pwr1_010.cfile', expected_data)
        nextSeriesSetting = 4
        testResults.append([])

    ###############################################################################################
    #   Unit tests series 4                                                                       #
    #        cr4-7_bw125_sf7_crc1_pwr1                                                            #
    ###############################################################################################
    #
    #   Unit test 35
    #
    def test_035 (self):
        expected_data = ["80 0b 01 01 23 45 67 89 ab cd ef"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_000.cfile', expected_data)

    #
    #   Unit test 36
    #
    def test_036 (self):
        expected_data = ["30 0b 02 11 11 11"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_001.cfile', expected_data)

    #
    #   Unit test 37
    #
    def test_037 (self):
        expected_data = ["30 0b 02 11 11 11"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_002.cfile', expected_data)

    #
    #   Unit test 38
    #
    def test_038 (self):
        expected_data = ["40 0b 07 aa aa aa aa"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_003.cfile', expected_data)

    #
    #   Unit test 39
    #
    def test_039 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_004.cfile', expected_data)

    #
    #   Unit test 40
    #
    def test_040 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_005.cfile', expected_data)

    #
    #   Unit test 41
    #
    def test_041 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_006.cfile', expected_data)

    #
    #   Unit test 42
    #
    def test_042 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_007.cfile', expected_data)

    #
    #   Unit test 43
    #
    def test_043 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_008.cfile', expected_data)

    #
    #   Unit test 44
    #
    def test_044 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_009.cfile', expected_data)

    #
    #   Unit test 45
    #
    def test_045 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-7_bw125_sf7_crc1_pwr1_010.cfile', expected_data)
        nextSeriesSetting = 5
        testResults.append([])

    ###############################################################################################
    #   Unit tests series 5                                                                       #
    #        cr4-6_bw125_sf6_crc1_pwr1                                                            #
    ###############################################################################################
    #
    #   Unit test 46
    #
    def test_046 (self):
        expected_data = ["80 0b 01 01 23 45 67 89 ab cd ef"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_000.cfile', expected_data)

    #
    #   Unit test 47
    #
    def test_047 (self):
        expected_data = ["30 0b 02 11 11 11"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_001.cfile', expected_data)

    #
    #   Unit test 48
    #
    def test_048 (self):
        expected_data = ["30 0b 02 11 11 11"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_002.cfile', expected_data)

    #
    #   Unit test 49
    #
    def test_049 (self):
        expected_data = ["40 0b 07 aa aa aa aa"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_003.cfile', expected_data)

    #
    #   Unit test 50
    #
    def test_050 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_004.cfile', expected_data)

    #
    #   Unit test 51
    #
    def test_051 (self):
        expected_data = ["40 0b 07 ff ff ff ff"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_005.cfile', expected_data)

    #
    #   Unit test 52
    #
    def test_052 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 3
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_006.cfile', expected_data)

    #
    #   Unit test 53
    #
    def test_053 (self):
        expected_data = ["40 0b 07 55 55 55 55"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_007.cfile', expected_data)

    #
    #   Unit test 54
    #
    def test_054 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 1
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_008.cfile', expected_data)

    #
    #   Unit test 55
    #
    def test_055 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 5
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_009.cfile', expected_data)

    #
    #   Unit test 56
    #
    def test_056 (self):
        expected_data = ["40 0b 07 88 88 88 88"] * 10
        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-6_bw125_sf6_crc1_pwr1_010.cfile', expected_data)
        self.lastTestComplete = True
        # nextSeriesSetting = 6
        # testResults.append([])

if __name__ == '__main__':
    testResults = [ [] ]

    nextSeriesSetting = 0
    test_series = [ TestSerieSettings("cr4-5_bw125_sf7_crc1_pwr1",   7),
                    TestSerieSettings("cr4-5_bw125_sf7_crc1_pwr1",   7),
                    TestSerieSettings("cr4-5_bw125_sf12_crc1_pwr1", 12),
                    TestSerieSettings("cr4-5_bw125_sf8_crc1_pwr1",   8),
                    TestSerieSettings("cr4-7_bw125_sf7_crc1_pwr1",   7),
                    TestSerieSettings("cr4-6_bw125_sf6_crc1_pwr1",   6)
                   ]

    # Remove second argument, otherwise this would cause each test to run twice
    # (once to console and once to xml, both outputs are printed in the console though)
    gr_unittest.run(qa_BasicTest)  # , "qa_BasicTest.xml")
