import lora, socket, pmt, time

from gnuradio import gr, gr_unittest, blocks
from gnuradio.blocks import socket_pdu

### Expected test results
#                    FILE                     |                 HEX
# --------------------------------------------+-------------------------------------
# hackrf_cr4-5_bw125_sf7_crc1_pwr1_001.cfile  | 40 0b 07 de ad be ef
# hackrf_cr4-5_bw125_sf7_crc1_pwr1_002.cfile  | (40 0b 07 de ad be ef) * 10
# hackrf_cr4-5_bw125_sf7_crc1_pwr1_003.cfile  | 80 0b 01 a5 a5 a5 a5 a5 a5 a5 a5
# hackrf_cr4-5_bw125_sf7_crc1_pwr1_004.cfile  | (80 0b 01 a5 a5 a5 a5 a5 a5 a5 a5) * 3

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
                print '% 3d :: match:\n %s' % (idx, val)
            else:
                print '% 3d :: mismatch:\n %s' % (idx, data_str)
                print 'should be:\n %s' % (expected_data[idx])

        print '\nPassed rate: %d out of %d (%.2f%%)\n' % (total_passing, len(expected_data), total_passing / len(expected_data) * 100.0)
        
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
    #    1. Set variables for various blocks
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
        self.server.setblocking(0)

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
    def tearDown (self):
        self.tb = None
        self.server.close()

        
    ###############################################################################################
    #   Unit tests                                                                                #
    #       These assume a directory "lora-samples" with the specified .cfiles existing.          #
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

    #
    #   Unit test 2
    #
    def test_002 (self):
        expected_data = [
            '40 0b 07 de ad be ef'
        ]

        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_001.cfile', expected_data)

    #
    #   Unit test 3
    #
    def test_003 (self):
        expected_data = [
            '40 0b 07 de ad be ef'
        ] * 10

        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_002.cfile', expected_data)

    #
    #   Unit test 4
    #
    def test_004 (self):
        expected_data = [
            '80 0b 01 a5 a5 a5 a5 a5 a5 a5 a5'
        ]

        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_003.cfile', expected_data)

    #
    #   Unit test 5
    #
    def test_005 (self):
        expected_data = [
            '80 0b 01 a5 a5 a5 a5 a5 a5 a5 a5'
        ] * 3

        self.runWithFileSourceAndData('/home/william/lora-samples/hackrf_cr4-5_bw125_sf7_crc1_pwr1_004.cfile', expected_data)

if __name__ == '__main__':
    # Remove second argument, otherwise this would cause each test to run twice 
    # (once to console and once to xml, both outputs are printed in the console though)
    gr_unittest.run(qa_BasicTest)  # , "qa_BasicTest.xml")
