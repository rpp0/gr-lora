#!/usr/bin/env python2
# -*- coding: utf-8 -*-
#
# Copyright 2016 Pieter Robyns.
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

# Notes from EP2763321A1, see https://www.google.com/patents/EP2763321A1

# General
# One chirp is one symbol
# The phase stays the same during a chirp, but can change between symbols (modulated chirps)
# Chirp duration may vary
# SF: 12 = N
# CR: 4/5
# BW: 125

# Modulation
# The value of the cyclic shift can be coded over log2 N bits, noted PPM.
# When a reduced set is used (N is small), the Gray mapper sets the least significant bits to zero. For a reduced set in which two bits are set to zero, hence using only one quarter of the available symbols, we have PPM = log2 N - 2, for example.
# The errors of ± 1 in the demodulated position have a much higher probability of occurring. Hence, there is a higher error probability for least significant bits: bit0 sees twice as much errors as bit1, which sees twice as much as bit2, and so on.
# See https://patentimages.storage.googleapis.com/EP2763321A1/imgf0002.png
# Each symbol is a chirp, and a number represents a codeword. M is one modulated bit (cycle shift)

# Sync
# In the example shown, the receiver looks for the presence of 3 consecutive symbols: unmodulated, modulated with first value, modulated with second value (for example a first chirp with a modulation value of 4, and a second one with the opposite modulation N - 4). Since the reception errors are mostly ± 1 offsets, it would not be advisable choosing these values for the frame synchronisation symbols. Modulation values are predetermined.
# According to another aspect of the invention, the preamble includes preferably frequency synchronisation symbols 413 that consist in one or more, preferably two chirps that are complex-conjugate of the base unmodulated chirp. One can regard these in the baseband representation as down-chirps
# Interesting remark in [0045]
# To let the receiver align in time, a silence 420 is inserted after the symbols 413.
# Finetuning described in [0050]

# PHDR
# The header part of the frame is a data field 415 that describes how to decode the information bits. The header content determines the decoding of large sections of the message and, therefore, it is preferably modulated with a reduced set of cyclic shifts, for example one possible position every four positions, i.e. PPM = log(N)-2. Since the most likely demodulation errors are ± 1 modulation position offset, this reduces significantly the probability that the header is not correctly decoded. Different reduction factors are also possible and included in the scope of the invention. A reduced set which is one third of the total set is also feasible, but this would not give an integer number of bits per symbol when the symbol size is, as it is usually a power of two. This technique of encoding part of the chirps with a reduced set of cyclic shifts can be used in other sensitive part of the transmitted data, whenever the likelihood of decoding errors is needed to be reduced.
# Preferably, the header 415 is encoded using the lowest coding rate, RDD=4, so as to give best protection, and comprises a header CRC to check header data integrity.
# The header 415 can include all sort of data useful to decode the payload. According to a possible implementation of the invention, the header fields are coding rate, enable of payload CRC, payload length, CRC, a burst mode indication, a compressed mode indication, some reserved bits, and a ranging bit. When the ranging bit is set, the header data has different meaning except header CRC.

# Pilot symbols
# According to an aspect of the invention, with an aim of improving the robustness without sacrificing the data rate, the payload includes "pilot symbols" as illustrated in Figure 7. These are symbols which use reduced modulation set, within the payload. In the represented example, in which the coding rate is
# 4/5, symbols form coding groups 512 containing 5 adjacent symbols (see next). One every 4 such groups can be encoded with a reduced modulation set 511, i.e. 5 symbols every 20 symbols. This is enough for a simple receiver to track timing, without compromising data rate. The repetition rate of such pilot symbols is a configuration known by both receiver and transmitter and is, for example, made known to the receiver by an appropriate field in the header.

import numpy
import struct
from gnuradio import gr
from scipy import signal

DELAY_AFTER_SYNC = 268  # Delay window after two downchirps in the LoRa signal
EPSILON = 0.001
VERBOSITY = 1

# Conditional print
def printd(string, level=2):
    if VERBOSITY >= level:
        print(string)

# Convert Gray code to binary
def ungray(gray):
    code_len = len(gray)
    gray = int(gray, 2)

    mask = gray >> 1
    binary = gray
    while mask != 0:
        binary = binary ^ mask
        mask = mask >> 1

    binary_str = bin(binary)[2:]
    return "0" * (code_len - len(binary_str)) + binary_str

# XOR binary number with PRNG byte and return binary
def xor_byte(number, prng):  # "number" should be a binary string, prng an integer from 0 - 255
    number_int = int(number, 2)
    binary = number_int ^ prng
    binary_str = bin(binary)[2:]
    return "0" * (8 - len(binary_str)) + binary_str

# xor_byte for a bitstring
def xor_string(string, prng):
    t = ""
    count = 0
    for i in range(0, len(string), 8):
        t += xor_byte(string[i:i+8], prng[count])
        count += 1
    return t

# Binary to Gray code
def gray(gray):
    code_len = len(gray)
    gray = int(gray, 2)

    binary = (gray >> 1) ^ gray

    binary_str = bin(binary)[2:]
    return "0" * (code_len - len(binary_str)) + binary_str

# Binary string to bytes
def bin_to_bytes(bin_str):
    result = b""
    for i in range(0, len(bin_str), 8):
        result += bytes(bytearray([int(bin_str[i:i+8], 2)]))
    return result

# Bytes to binary string
def bytes_to_bin(byte_str):
    result = ''
    binary_str = ''
    for elem in byte_str:
        binary_str = bin(elem)[2:]
        binary_str = "0" * (8 - len(binary_str)) + binary_str
    result += binary_str
    return result

# Binary string to hex string
def bin_to_hex(bin_str, manchester=False):
    str_len = len(bin_str)

    if str_len % 8 != 0:
        printd("Padding string of len " + str(str_len))
        #bin_str = ("0" * (str_len % 8)) + bin_str  # Pad at begin
        bin_str = bin_str + ("0" * (str_len % 8))  # Pad at end


    if manchester:
        t = ""
        for i in range(0, str_len, 2):
            substr = bin_str[i:i+2]
            if substr == "10":
                t += "1"
            elif substr == "01":
                t += "0"
            else:
                print("Warning: not Manchester. Decoded value will be wrong")
        bin_str = t

    result = ""
    for i in range(0, str_len, 8):
        substr = bin_str[i:i+8]
        hchr = hex(int(substr, 2)).split('0x')[1]
        if len(hchr) == 1:
            hchr = "0" + hchr
        result += hchr + " "
    return result.strip()

# Hamming helper functions
def _check_parity(word, even=True):
    count = 0
    for i in range(0, len(word)):
        if word[i] == "1":
            count += 1
    if even:
        return ((count % 2) == 0)
    else:
        return (((count+1) % 2) == 0)

def _flip(bit):
    if bit == "0":
        return "1"
    else:
        return "0"

def hamming_onlydata(string, indices):
    result = ""
    for i in range(0, len(string)):
        if i in indices:
            result += string[i]
    return result

def hamming_decode(word, powers_of_two_pos=[0, 1, 3], even=True):
    error = 0
    for i in powers_of_two_pos:
        j = i
        to_check = ""
        while j < len(word): # j: position after skip. k: position to append
            k = j
            while k < j+i+1 and k < len(word):
                to_check += word[k]
                k += 1
            j += ((i+1)*2)
        #print("Checking p%d: %s" % (i+1, str(to_check)))
        if not _check_parity(to_check, even):
            #print("Found incorrect parity at %d" % i)
            error += (i+1)

    # Correct errors here
    if error > 0:
        index = error - 1
        printd("Error at index %d" % index)
        word = word[:index] + _flip(word[index]) + word[index+1:]

    data = ""
    for i in range(0, len(word)):
        bit = word[i]
        if not i in powers_of_two_pos:
            data += bit

    return data, error

def shuffled_hamming(coded, prng):
    bit_order = (7, 6, 3, 4, 2, 1, 0, 5)  # Reverse engineered permutation

    # Deshuffle
    deshuffle = ""
    for bit in bit_order:
        deshuffle += coded[bit]

    printd("S: " + str(deshuffle))

    # Dewhiten
    dewhiten = xor_byte(deshuffle, prng)

    printd("W: " + str(dewhiten))

    # Decode
    decode, error = hamming_decode(dewhiten[:-1], powers_of_two_pos=[0, 1, 3], even=True)

    return decode, error

# State machine states
class LoRaState:
    FIND_SYNC = 0
    VERIFY_SYNC = 1
    DECODE_HDR = 2
    DECODE_PAYLOAD = 3

# GNURadio block
class lora_decoder(gr.basic_block):
    """
    docstring for block lora_decoder
    """
    def __init__(self):
        gr.basic_block.__init__(self,
            name="lora_decoder",
            in_sig=[numpy.float32, numpy.complex64],
            out_sig=[numpy.float32, numpy.float32])

        self.last_sample = 0.0
        self.sync_buffer = []
        self.downchirp = []
        self.window_size = 0
        self.sample_buffer = []

        self.sf = 7  # Seems to only affect PHY radio send?
        self.bw = 125000.0
        self.cr = 4
        self.state = 0
        self.bits_per_second = self.sf * 1 / ((2**self.sf) / self.bw)
        self.samples_per_second = 1000000
        self.symbols_per_second = self.bw / (2**self.sf)
        self.bits_per_symbol = int(self.bits_per_second / self.symbols_per_second)
        self.samples_per_symbol = int(self.samples_per_second / self.symbols_per_second)
        self.number_of_bins = int('0b' + '1' * self.sf, 2) + 1
        self.number_of_bins_hdr = self.number_of_bins / 4
        self.compression = 8
        self.payload_symbols = 0
        print("Bits per symbol: " + str(self.bits_per_symbol))
        print("Bins per symbol: " + str(self.number_of_bins))
        print("Header bins per symbol: " + str(self.number_of_bins_hdr))
        print("Samples per symbol: " + str(self.samples_per_symbol))

        self.words = []
        self.demodulated = ""

        # Generate ideal downchirp
        t = numpy.arange(self.samples_per_symbol) / float(self.samples_per_second)
        beta = -1 * (self.bw / (t[-1] - t[0]))
        alpha = (self.bw / 2.0) + 0
        self.downchirp = numpy.exp(2.0 * numpy.pi * 1j * (alpha * t + 0.5 * beta * t**2)).astype(numpy.complex64)

    def deinterleave(self, words, ppm):
        bits_per_word = len(words)

        result = []

        offset_start = 0
        for i in range(0, ppm):
            d = ""
            offset_diag = offset_start
            for j in range(0, bits_per_word):
                d += words[j][offset_diag]
                offset_diag = (offset_diag + 1) % ppm
            offset_start += 1
            result.append(d)

        return result

    def forecast(self, noutput_items, ninput_items_required):
        ninput_items_required[0] = noutput_items

    def compress(self, samples, amt):
        return numpy.mean(samples.reshape(-1, amt), axis=1)

    def gradient(self, samples):
        instantaneous_phase = numpy.unwrap(numpy.angle(samples))
        instantaneous_frequency = (numpy.diff(instantaneous_phase) / (2.0*numpy.pi) * self.samples_per_second)
        c_freq = self.compress(instantaneous_frequency, self.compression)
        gradient = numpy.gradient(c_freq)
        return numpy.abs(gradient)

    def nibble_reverse(self, data):
        result = ""
        for i in range(0, len(data), 8):
            result += data[i+4:i+8]
            result += data[i+0:i+4]
        return result

    # 1. Find min and max values of cyclic shift
    # 2. Apply Gray code to index of the transient of the change in instantaneous frequency
    # 3. Read 4 + RDD (redundancy bits) symbols
    # 4. Deinterleave
    # 5. De-whiten
    # 6. Hamming decode
    def demodulate(self, samples, is_hdr, out, out_w):
            # Possibility 1: convolve in freq domain
            #mult = numpy.convolve(...)

            # Possibility 2: multiply and FFT
            #mult = numpy.multiply(samples, self.downchirp)
            #xx = numpy.array(samples)
            #xx.tofile("/tmp/in1")
            #self.downchirp.tofile("/tmp/downchirp")
            #mult.tofile("/tmp/mult")
            #mult = numpy.fft.fft(mult, self.window_size)
            #mult = numpy.abs(mult)

            # Possibility 3: just use gradient of upchirp (faster?)
            gradient_mag = self.gradient(samples)

            result = (int(numpy.argmax(gradient_mag)) / (8 / self.compression)) # TODO scale according to bins e.g. 128 = divide by 8 / compression, i.e. divide by header bins per symbol!
            # Header bins
            if is_hdr == True:
                result /= 4

            # Convert argmax to binary
            result_binary = bin(result)[2:]

            # 5+3 coding for header
            if is_hdr == True:
                result_binary = "0" * (self.sf - 2 - len(result_binary)) + result_binary # -2 to cut off LSB of gray code
            else:
                result_binary = "0" * (self.sf - len(result_binary)) + result_binary

            # Convert TO Gray
            result_binary = gray(result_binary)

            # Debug info for Audacity
            printd(str(result) + " " + result_binary)
            for i in range(0, self.window_size):
                out[i] = gradient_mag[i/self.compression] / numpy.max(gradient_mag) * 100.0 # FFT of convolution result
                if is_hdr == True:
                    out_w[i] = (float(result) / self.number_of_bins_hdr) * 100.0  # Resulting bin scaled
                else:
                    out_w[i] = (float(result) / self.number_of_bins) * 100.0  # Resulting bin scaled
                #out_w[i] = numpy.abs(in1[i] * 2+2j) # Real data

            self.words.append(result_binary)

            # Look for 4+cr symbols and stop
            if len(self.words) == (4 + self.cr):
                printd(self.words)

                # Deinterleave
                if is_hdr == True:
                    deinterleaved = self.deinterleave(self.words, self.sf - 2)
                else:
                    deinterleaved = self.deinterleave(self.words, self.sf)

                deinterleaved.reverse()  # Words are transmitted in reverse order. TODO is this because I missed something in an earlier stage?
                deinterleaved = ''.join(deinterleaved)
                printd("D: " + deinterleaved)

                self.words = []
                self.demodulated += deinterleaved
                return deinterleaved
            return None

    def decode(self, is_hdr):
        printd("DEMO: " + self.demodulated)

        data = ""
        if is_hdr:
            prng = [0x44, 0x88, 0x00, 0x00, 0x00]  # TODO: Last 3 bytes still unknown
        else:
            # TODO: Probably not 100% correct towards the end, because of clock drift. Needs to be corrected
            prng = [0xfb, 0xf7, 0xd, 0xef, 0x79, 0xbf, 0x63, 0xfb, 0x8, 0xdf, 0xc2, 0x2c, 0x15, 0xfa, 0x4f, 0xe9, 0x46, 0x10, 0x1f, 0x92, 0x7d, 0x51, 0x25, 0x6b, 0x68, 0x67, 0xa1, 0x9c, 0xd6, 0xf7, 0xa7, 0x3d, 0xcd, 0x38, 0x28, 0xe5, 0xbc, 0xf2, 0x89, 0x86, 0xa1, 0x4e, 0x4, 0xa2, 0x20, 0xa4, 0x1, 0xd8, 0x82, 0xe5, 0xda, 0x20, 0x45, 0x1, 0xd9, 0x82, 0xe5, 0x8, 0xf2, 0xc2, 0x86, 0x15, 0x9c, 0x9d, 0x25, 0xc1, 0x68, 0xd3, 0xa1, 0x5, 0xd6, 0x16, 0x75, 0x3d, 0x98, 0x38, 0xb1, 0xe5, 0x5d, 0xf2, 0x5b, 0x54, 0x26, 0x1b, 0x1a, 0x3b, 0xd, 0x45, 0x79, 0xb, 0x63, 0x62, 0x8, 0x3e, 0x10, 0x2c, 0x40, 0xfa, 0xd6, 0xe9, 0xa7, 0x10, 0xcd, 0x40, 0xfa, 0x4, 0x3b, 0xf2, 0x45, 0x86, 0xd9, 0x4e, 0x37, 0x70, 0x75, 0x23, 0x98, 0x15, 0xb1, 0x9d, 0x8f, 0x13, 0xdc, 0x54, 0xea, 0x1b, 0x62, 0x3b, 0x3e, 0x97, 0x2c, 0x8c, 0xfa, 0xae, 0x3b, 0x46, 0x45, 0x1f, 0xd9, 0xaf, 0xe5, 0x70, 0x20, 0x3, 0x1, 0x15, 0x50, 0x9d, 0x5d, 0xc5, 0x5b, 0x1, 0xf4, 0xc2, 0x4f, 0x9e, 0x46, 0x45, 0xcd, 0xb, 0x28, 0xb0, 0x6e, 0x69, 0xd4, 0x77, 0xeb, 0x4e, 0xb0, 0x22, 0x69, 0x7e, 0x77, 0x8d, 0xe, 0xee, 0xf4, 0x9c, 0x3, 0x4a, 0x14, 0xe4, 0xd, 0xc0, 0xce, 0x68, 0xfd, 0x73, 0xfb, 0x93, 0xed, 0x6b, 0x18, 0x67, 0xb3, 0x5e, 0xc2, 0xf2, 0x3e, 0xfd, 0xde, 0x50, 0x7d, 0xd6, 0xa3, 0xa7, 0xba, 0x27, 0x27, 0x3d, 0xda, 0x73, 0x8b, 0x15, 0x67, 0xea, 0xdc, 0x20, 0xf3, 0x65, 0x3d, 0x9b, 0xea, 0xe4, 0xa2, 0x96, 0x67, 0xef, 0x47, 0x6c, 0xdc, 0xee, 0xa7, 0x91, 0xba, 0x62, 0xf4, 0xf4, 0x9e, 0x92, 0xc1, 0x1d, 0xdb, 0xfa, 0x97, 0xa0, 0x11, 0xb8, 0xd9, 0xe0, 0x40, 0x10, 0xd4, 0x99, 0xaf, 0x89, 0xdd, 0xa1, 0x9, 0x6, 0x6e, 0xfe, 0x5c, 0x86, 0x38, 0x3e, 0x77, 0xe3, 0xfa, 0x56, 0x82, 0x8c, 0x9c, 0x7f, 0xf3, 0x41, 0x1d, 0xdb, 0x28, 0x45, 0x35, 0x16, 0xab, 0x6f, 0x3f, 0x7c, 0xef, 0x7e, 0x34, 0x1b, 0x23, 0xb6, 0x47, 0x9d, 0x48, 0x25, 0x5c, 0x40, 0xe0, 0x72, 0x2, 0x40, 0x5d, 0xe0, 0x7b, 0x79, 0x36, 0x63, 0x4a, 0x9e, 0x8a, 0x4d, 0xb5, 0x3a, 0x1b, 0x60, 0x3b, 0xe4, 0xc1, 0xab, 0xf8, 0x66, 0x6f, 0x91, 0xf2, 0x23, 0x64, 0x11, 0x88, 0x1d, 0xbc, 0xc5, 0xa9, 0xdb, 0x60, 0x97, 0x93, 0x47, 0x6b, 0xac, 0x95, 0xc8, 0xc9, 0xb5, 0xb8, 0x75, 0xc9, 0x42, 0x72, 0x6, 0x83, 0xc1, 0x36, 0x80, 0x3c, 0xd9, 0x69, 0xb5, 0x6e, 0xa3, 0xe, 0x7, 0x6d, 0xce, 0x6c, 0xa2, 0x4e, 0x52, 0xcd, 0x5e, 0xea, 0xeb, 0x3f, 0x8a, 0x9f, 0x67, 0x7f, 0x5e, 0x79, 0x70, 0xd, 0x2f, 0x9b, 0xc7, 0xa5, 0x1a, 0x14, 0xd3, 0x4f, 0x3c, 0xbf, 0x69, 0xf9, 0xee, 0x1, 0xe, 0xab, 0x9e, 0x66, 0x79, 0xc1, 0xd7, 0x7, 0x2c, 0xc7, 0x29, 0xca, 0x78, 0x50, 0x89, 0x22, 0x53, 0x4, 0x0, 0xc4, 0xe8, 0xb2, 0x49, 0xf4, 0xf1, 0x4c, 0x8b, 0x14, 0xfc, 0x90, 0xfa, 0x23, 0xb2, 0x5a, 0x63, 0x97, 0xb1, 0xbd, 0x5b, 0x7c, 0xbd, 0x49, 0xe2, 0x3, 0xc6, 0x82, 0xca, 0x1e, 0x50, 0x97, 0xf1, 0x7e, 0x82, 0x69, 0xc, 0x82, 0x60, 0xb5, 0xd6, 0xc9, 0x18, 0xb8, 0x8d, 0x72, 0xab, 0xf4, 0x66, 0x4f, 0x41, 0x4e, 0x80, 0x8f, 0xb, 0x3c, 0x30, 0x73, 0x21, 0x1d, 0x67, 0x39, 0x8c, 0xb7, 0xf1, 0x35, 0xcf, 0x52, 0x6d, 0x37, 0x3c, 0x95, 0x9b, 0xd3, 0x54, 0x51, 0xc9, 0x54, 0xb8, 0x71, 0xa9, 0x90, 0xb0, 0xf1, 0x4, 0x8f, 0x7c, 0x2e, 0x44, 0xbf, 0x5a, 0xfb, 0xbd, 0xdf, 0x9f, 0xee, 0x7f, 0xc2, 0x44, 0x8, 0x30, 0x87, 0x67, 0xca, 0x46]

        prng_i = 0
        errors = 0
        for i in range(0, len(self.demodulated), 8):
            d,e = shuffled_hamming(self.demodulated[i:i+8], prng[prng_i])
            data += d
            errors += e
            if prng_i < len(prng) - 1:
                prng_i += 1

        # Nibbles are reversed
        data = self.nibble_reverse(data)

        # Remove decoded bits from demodulated data array
        self.demodulated = ""

        printd("DATA: " + bin_to_hex(data, False) + " (" + str(errors) + ")")
        return data, errors

    def reset(self):
        self.demodulated = ""
        self.state = LoRaState.FIND_SYNC

    def calc_energy(self, samples):
        return numpy.sum(numpy.square(numpy.abs(samples)))

    def general_work(self, input_items, output_items):
        in0 = input_items[0]
        in1 = input_items[1]
        out = output_items[0]
        out_w = output_items[1]
        num_output_items = len(output_items[0])

        if self.state == LoRaState.FIND_SYNC:
            #if self.calc_energy(in1[0:32]) > 0.08:
            #    print("Found packet")
            smooth_function = numpy.convolve(signal.gaussian(32, std=12), in0, mode='same') / 32.0
            for i in range(0, num_output_items):
                out[i] = len(self.sync_buffer) / 10
                out_w[i] = smooth_function[i] * 100

                if smooth_function[i] < self.last_sample + EPSILON:
                    self.sync_buffer.append(in1[i])
                else:
                    if len(self.sync_buffer) > 900:
                        self.window_size = self.samples_per_symbol
                        printd("SYNC %d (%d)" % (self.window_size, len(self.sync_buffer)))  # Sync buffer is how long we observed the chirp to be

                        self.state = LoRaState.VERIFY_SYNC

                        # Get downchirp from preamble
                        #for j in range(0, len(self.sync_buffer)):
                        #    self.downchirp.append(self.sync_buffer[j])
                        self.sync_buffer = []

                        self.set_output_multiple(self.window_size + DELAY_AFTER_SYNC)
                        self.consume_each(i+1)
                        return i+1

                    # Add sync hits to array
                    self.sync_buffer = []

                self.last_sample = smooth_function[i]

            self.consume_each(num_output_items)
            return num_output_items

        # TODO: Fourier transform to check second sync
        elif self.state == LoRaState.VERIFY_SYNC:
            #test = signal.correlate(in1[0:self.window_size], self.downchirp, mode='same')  # Divide by 10 for visualizing
            test = self.gradient(in1[0:self.window_size+1])
            for i in range(0, self.window_size):
                out[i] = 100
                out_w[i] = test[i/self.compression] / 100.0

            #xx = numpy.array(in1)
            #xx.tofile("/tmp/sync")
            #self.downchirp.tofile("/tmp/downchirp")

            corr_argmax = numpy.argmax(test)
            corr_max = numpy.max(test)
            printd("Sync corr:" + str(corr_max) + " " + str(corr_argmax))

            if corr_argmax > 2 or numpy.mean(test) > 2250.0:
                printd("Lost sync!")
                self.state = LoRaState.FIND_SYNC
                self.consume_each(self.window_size)
                return self.window_size
            else:
                printd("Decoding")
                self.state = LoRaState.DECODE_HDR
                for j in range(self.window_size, self.window_size + DELAY_AFTER_SYNC):
                    out[j] = 80
                    out_w[j] = 80
                self.consume_each(self.window_size + DELAY_AFTER_SYNC)
                return self.window_size + DELAY_AFTER_SYNC

        # Decode header
        elif self.state == LoRaState.DECODE_HDR:
            demodulated = self.demodulate(in1[0:self.window_size+1], True, out, out_w)
            if not demodulated is None:
                header, errors = self.decode(is_hdr=True)
                payload_length = int(header[4:8] + header[0:4], 2)  # TODO: Why are these nibbles reversed *again*?

                # Set new CR for payload
                self.cr = 4 # TODO: Get from header instead of hardcode

                symbols_per_block = self.cr + 4
                bits_needed = ((payload_length * 8) + 16) * (symbols_per_block / 4)
                symbols_needed = float(bits_needed) / float(self.sf)
                blocks_needed = numpy.ceil(symbols_needed / symbols_per_block)
                self.payload_symbols = blocks_needed * symbols_per_block
                printd("LEN: " + str(payload_length) + " (" + str(self.payload_symbols) + " symbols)")

                self.state = LoRaState.DECODE_PAYLOAD

            self.consume_each(self.window_size)
            return self.window_size

        elif self.state == LoRaState.DECODE_PAYLOAD:
            decoded = self.demodulate(in1[0:self.window_size+1], False, out, out_w)
            if not decoded is None:
                self.payload_symbols -= (4 + self.cr)
                if self.payload_symbols <= 0:
                    payload, errors = self.decode(is_hdr=False)
                    print(bin_to_hex(payload, False) + "   (" + str(errors) + " errors)")
                    self.reset() # Ready for new packet

            self.consume_each(self.window_size)
            return self.window_size
