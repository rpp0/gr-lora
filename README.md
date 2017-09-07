gr-lora [![Build status](https://api.travis-ci.org/rpp0/gr-lora.svg)](https://travis-ci.org/rpp0/gr-lora) [![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.853201.svg)](https://doi.org/10.5281/zenodo.853201)
=======

The gr-lora project aims to provide a collection of GNU Radio blocks for receiving LoRa modulated radio messages using a Software Defined Radio (SDR). More information about LoRa itself can be found on [the website of the LoRa Alliance](https://www.lora-alliance.org/).

![alt text](https://github.com/rpp0/gr-lora/blob/master/examples/screenshot.png "gr-lora example")

## Features

All features of the LoRa physical-layer modulation scheme are described in various patents and blog posts (for a good resource, see [this RevSpace page](https://revspace.nl/DecodingLora)). ```gr-lora``` supports most of these features, except for:

- CRC checks of the payload and header
- Decoding multiple channels simultaneously

This library was primarily tested with a USRP B201 as receiver and Microchip RN2483 as transmitter. If you encounter an issue with your particular setup, feel free to let me know in the 'Issues' section of this repository.


### Update of 29th August, 2017

I'm happy to announce that as of ```gr-lora``` version 0.6, a new clock recovery algorithm has been implemented which fixes previous issues with long LoRa messages. Other components, such as whitening, detection and decoding have been improved as well (see the Git log for more details). Given a clear signal, the decoding accuracy is now [close to 100% for all SFs](https://github.com/rpp0/gr-lora/tree/master/docs/test-results), and I therefore consider LoRa fully reverse engineered. Future updates will focus on improving the performance and minor details of the specification.


## Attribution

If you are working on a research topic or project that involves the usage of ```gr-lora``` or its algorithms, we would appreciate it if you could acknowledge us. We are currently working on a full writeup of the decoder, but in the meantime, you can cite this repository as follows:

> Pieter Robyns, Peter Quax, Wim Lamotte, William Thenaers. (2017). gr-lora: An efficient LoRa decoder for GNU Radio. Zenodo. 10.5281/zenodo.853201


## Installation

Installing `gr-lora` is possible in two ways: either by downloading the Docker container, which contains all dependencies and `gr-lora` packaged in a single container, or by manual installation.

### Docker installation

To avoid installation errors and cluttering your system with the required dependencies, the best approach to install `gr-lora` is through a Docker container. Make sure the `docker` service is running, then perform the following steps:

```
git clone https://github.com/rpp0/gr-lora.git .
cd docker/
./docker_run_grlora.sh
```

The `docker_run_grlora.sh` script will download the Docker container and run it in 'privileged' mode (in order to access your SDR over USB). After that, you should get a shell inside the container:

```
[root@5773ed19d95d apps]#
```

See the 'Testing' section below for examples on how to use `gr-lora`.

### Manual installation

If you prefer a manual installation, the following dependencies are required: `python2-numpy`, `python2-scipy`, `swig`, `cppunit`, `fftw`, `gnuradio`, `libvolk`, `log4cpp`, `cmake`, `wx`, and [`liquid-dsp`](https://github.com/jgaeddert/liquid-dsp).

The installation procedure is the same as for any GNU Radio OOT module:

```
git clone https://github.com/rpp0/gr-lora.git .
mkdir build
cd build
cmake ../  # Note to Arch Linux users: add "-DCMAKE_INSTALL_PREFIX=/usr"
make && sudo make install
```

## Testing and usage

To test your installation, you can download one of the example LoRa signals at [rpp0/gr-lora-samples](https://github.com/rpp0/gr-lora-samples). Configure ```apps/lora_receive_file.py``` to use the sample and run the script. You should see the decoded sample data:

```
$ ./lora_receive_file.py

Bits (nominal) per symbol: 7
Bins per symbol: 128
Header bins per symbol: 32
Samples per symbol: 1024
Using Volk machine: avx2_64_mmx_orc
00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22
88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88 88
12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12
...
```

Alternatively, if you have a hardware LoRa transmitter, you use ```apps/lora_receive_realtime.py``` to decode signals in real time. If you have a Microchip RN2483, you can use [python-loranode](https://github.com/rpp0/python-loranode) to easily send messages via Python.

By default, decoded messages will be printed to the console output. However, you can use a `message_socket_sink` to forward messages to port 40868 over UDP. See the [tutorial](https://github.com/rpp0/gr-lora/wiki/Capturing-LoRa-signals-using-an-RTL-SDR-device) for more information.


## Contributing

Contributions to the project are very much appreciated! If you have an idea for improvement or noticed a bug, feel free to submit an issue. If you're up for the challenge and would like to introduce a feature yourself, we kindly invite you to submit a pull request.


## Hardware support

The following LoRa modules and SDRs were tested and work with gr-lora:

Transmitters: Pycom LoPy, Dragino LoRa Raspberry Pi HAT, Adafruit Feather 32u4, Microchip RN 2483 (custom board)
Receivers: HackRF One, USRP B201, RTL-SDR


## Changelog

- Version 0.6.1: Minor bug fixes and improvements.
- Version 0.6  : Significantly increased decoding accuracy and clock drift correction.
- Version 0.5  : Major overhaul of preamble detection and upchirp syncing
- Version 0.4  : Support for all spreading factors, though SFs 11 and 12 are still slow / experimental
- Version 0.3  : Support for all coding rates
- Version 0.2.1: Fixed some issues reported by reletreby
- Version 0.2  : C++ realtime decoder, manual finetuning for correcting frequency offsets of the transmitter.
- Version 0.1  : Python prototype file based decoder, SF7, CR4/8


## License

See the LICENSE file and top of the source files for the license of this project.
