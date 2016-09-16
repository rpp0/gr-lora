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

from gnuradio import gr
from lora_decoder import lora_decoder
from gnuradio.filter import freq_xlating_fir_filter_ccf, firdes, fractional_resampler_cc
from gnuradio.analog import quadrature_demod_cf
from gnuradio.blocks import null_sink
import lora

class lora_receiver(gr.hier_block2):
    """
    docstring for block lora_receiver
    """
    def __init__(self, in_samp_rate, freq, offset, sf, realtime):
        gr.hier_block2.__init__(self,
            "lora_receiver",  # Min, Max, gr.sizeof_<type>
            gr.io_signature(1, 1, gr.sizeof_gr_complex),  # Input signature
            gr.io_signature(0, 0, 0)) # Output signature

        # Parameters
        self.offset = offset
        self.sf = sf
        self.in_samp_rate = in_samp_rate
        self.realtime = realtime
        bw = 125000

        # Define blocks
        null1 = null_sink(gr.sizeof_float)
        null2 = null_sink(gr.sizeof_float)
        if realtime:
            self.c_decoder = lora.decoder(sf)
            out_samp_rate = 1000000
        else:
            decoder = lora_decoder()
            out_samp_rate = 10000000

        decimation = 1

        lpf = firdes.low_pass(1, out_samp_rate, 86000, 20000, firdes.WIN_HAMMING, 6.67)
        qdemod = quadrature_demod_cf(1.0)
        channelizer = freq_xlating_fir_filter_ccf(decimation, lpf, offset, out_samp_rate)
        self.channelizer = channelizer
        resampler = fractional_resampler_cc(0, float(in_samp_rate) / float(out_samp_rate))

        # Connect blocks
        self.connect((self, 0), (resampler, 0))
        self.connect((resampler, 0), (channelizer, 0))
        if realtime:
            self.connect((channelizer, 0), (self.c_decoder, 0))
        else:
            self.connect((channelizer, 0), (qdemod, 0))
            self.connect((channelizer, 0), (decoder, 1))
            self.connect((qdemod, 0), (decoder, 0))
            self.connect((decoder, 0), (null1, 0))
            self.connect((decoder, 1), (null2, 0))

    def set_sf(self):
        return self.sf

    def set_sf(self, sf):
        self.sf = sf
        if self.realtime:
            self.c_decoder.set_sf(self.sf)

    def get_offset(self):
        return self.offset

    def set_offset(self, offset):
        self.offset = offset
        self.channelizer.set_center_freq(self.offset)
