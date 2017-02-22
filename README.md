gr-lora
=======

The gr-lora project aims to provide a collection of GNURadio blocks for receiving LoRa modulated radio messages using Software Defined Radio (SDR). More information about LoRa itself can be found on [the website of the LoRa Alliance](https://www.lora-alliance.org/).

![alt text](https://github.com/rpp0/gr-lora/blob/master/examples/screenshot.png "gr-lora example")


Features
--------

Though there is no official reference implementation of LoRa, the protocol is described in various patents and blog posts (for a good resource, see [this RevSpace page](https://revspace.nl/DecodingLora)). The following features are fully supported by gr-lora:

- Synchronizing to LoRa frames transmitted with any spreading factor
- Deinterleaving
- Dewhitening
- Decoding + Hamming error correction of the LoRa PHDR length field
- Decoding + Hamming error correction of frame payloads (all coding rates)

Not supported yet:

- CRC checks of the payload and header
- Decoding multiple channels simultaneously
- Clock drift correction for very long frames

This feature set is enough for basic, experimental usage with SDRs.


Installation
------------

The installation procedure is the same as for any GNURadio OOT module:

```
mkdir build
cd build
cmake ../  # Note to Arch Linux users: add "-DCMAKE_INSTALL_PREFIX=/usr"
make && sudo make install
```

The following dependencies are required:
- numpy
- scipy
- [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)


Testing
-------

To test your installation, you can download one of the sample LoRa signals at [rpp0/gr-lora-samples](https://github.com/rpp0/gr-lora-samples). Configure ```apps/lora_receive_file.py``` to use the sample and run the script. You should see the decoded sample data:

```
$ ./lora_receive_file.py

Bits per symbol: 7
Bins per symbol: 128
Header bins per symbol: 32
Samples per symbol: 1024
Using Volk machine: avx2_64_mmx_orc
00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 b8 73 af 81 69
88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 fd e5 af 81 69
12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 a3 69 af 81 69
...
```

Alternatively, if you have a LoRa transmitter, you can configure/modify  ```/examples/_examplify.py``` to generate example files and add them to ```python/qa_BasicTest.py``` as Unit Test, or to ```/examples/qa_BasicTest_Data.xml``` to run them automatically with ```python/qa_BasicTest_XML.py``` (```xmltodict``` module needed).
Note that these can be run with their shell script in ```build\python```.


Contributing
------------

Contributions to the project are very much appreciated! If you have an idea for improvement or noticed a bug, feel free to submit an issue. If you're up for the challenge and would like to introduce a feature yourself, we kindly invite you to submit a pull request.


Hardware
--------

Primarily, gr-lora was developed using a USRP and RN2483 LoRa chip. The following LoRa modules and SDRs were also tested and work with gr-lora:

Transmitters: Pycom LoPy, Dragino LoRa Raspberry Pi HAT, Adafruit Feather 32u4, Microchip RN 2483 (custom board)
Receivers: HackRF One, USRP B201, RTL-SDR


Usage
-----

See the LICENSE file for the license of this project. If you are working on a project that involves the usage of gr-lora, we would appreciate it if you can acknowledge gr-lora by linking to this page.


Changelog
---------

- Version 0.4 : Support for all spreading factors, though SFs 11 and 12 are still slow / experimental
- Version 0.3 : Support for all coding rates
- Version 0.21: Fixed some issues reported by reletreby
- Version 0.2 : C++ realtime decoder, manual finetuning for correcting frequency offsets of the transmitter.
- Version 0.1 : Python prototype file based decoder, SF7, CR4/8
