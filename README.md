gr-lora [![Build status](https://api.travis-ci.org/rpp0/gr-lora.svg)](https://travis-ci.org/rpp0/gr-lora) [![DOI](https://zenodo.org/badge/61641535.svg)](https://zenodo.org/badge/latestdoi/61641535)
=======

The gr-lora project aims to provide a collection of GNU Radio blocks for receiving LoRa modulated radio messages using Software Defined Radio (SDR). More information about LoRa itself can be found on [the website of the LoRa Alliance](https://www.lora-alliance.org/).

![alt text](https://github.com/rpp0/gr-lora/blob/master/examples/screenshot.png "gr-lora example")


Update of 29th August, 2017
---------------------------

I'm happy to announce that as of ```gr-lora``` version 0.6, a new clock recovery algorithm has been implemented which fixes previous issues with long LoRa messages. Other components, such as whitening, detection and decoding have been improved as well (see the Git log for more details). Given a clear signal, the decoding accuracy is now close to 100% for all SFs, and I therefore consider LoRa fully reverse engineered. Future updates will focus on improving the performance and minor details of the specification.


Usage
-----

See the LICENSE file and top of the source files for the license of this project. If you are working on a research topic or project that involves the usage of ```gr-lora``` or its algorithms, we would appreciate it if you could acknowledge us. We are currently working on a full writeup of the decoder, but in the meantime, you can cite this repository as follows:

```
Pieter Robyns, Peter Quax, Wim Lamotte, William Thenaers. (2017). gr-lora: An efficient LoRa decoder for GNU Radio. Zenodo. 10.5281/zenodo.853201
```


Features
--------

All features of the LoRa physical-layer modulation scheme are described in various patents and blog posts (for a good resource, see [this RevSpace page](https://revspace.nl/DecodingLora)). ```gr-lora``` supports most of these features, except for:

- CRC checks of the payload and header
- Decoding multiple channels simultaneously

This library was primarily tested with a USRP B201 as receiver and Microchip RN2483 as transmitter. If you encounter an issue with your particular setup, feel free to let me know in the Issues section of this repository.


Installation
------------

The following dependencies are required:
- numpy
- scipy
- log4cpp
- [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)


The installation procedure is the same as for any GNURadio OOT module:

```
mkdir build
cd build
cmake ../  # Note to Arch Linux users: add "-DCMAKE_INSTALL_PREFIX=/usr"
make && sudo make install
```

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

Alternatively, if you have a LoRa transmitter, you can configure/modify  ```/examples/_examplify.py``` to generate example files and add them to ```/examples/qa_BasicTest_Data.xml``` to run them automatically with ```python/qa_BasicTest_XML.py``` (```xmltodict``` module needed). Note that this script should be run with its shell script in ```build/python```. This is to ensure compatibility with ```make test```.


Contributing
------------

Contributions to the project are very much appreciated! If you have an idea for improvement or noticed a bug, feel free to submit an issue. If you're up for the challenge and would like to introduce a feature yourself, we kindly invite you to submit a pull request.


Hardware
--------

The following LoRa modules and SDRs were also tested and work with gr-lora:

Transmitters: Pycom LoPy, Dragino LoRa Raspberry Pi HAT, Adafruit Feather 32u4, Microchip RN 2483 (custom board)
Receivers: HackRF One, USRP B201, RTL-SDR


Changelog
---------

- Version 0.6  : Significantly increased decoding accuracy and clock drift correction.
- Version 0.5  : Major overhaul of preamble detection and upchirp syncing
- Version 0.4  : Support for all spreading factors, though SFs 11 and 12 are still slow / experimental
- Version 0.3  : Support for all coding rates
- Version 0.2.1: Fixed some issues reported by reletreby
- Version 0.2  : C++ realtime decoder, manual finetuning for correcting frequency offsets of the transmitter.
- Version 0.1  : Python prototype file based decoder, SF7, CR4/8
