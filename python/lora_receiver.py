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
import lora

class lora_receiver(gr.hier_block2):
    """
    docstring for block lora_receiver
    """
    def __init__(self, in_samp_rate, center_freq, channel_list, sf, out_samp_rate, threshold = 0.01):
        gr.hier_block2.__init__(self,
            "lora_receiver",  # Min, Max, gr.sizeof_<type>
            gr.io_signature(1, 1, gr.sizeof_gr_complex),  # Input signature
            gr.io_signature(0, 0, 0)) # Output signature

        # Parameters
        self.in_samp_rate  = in_samp_rate
        self.center_freq   = center_freq
        self.sf            = sf
        self.out_samp_rate = out_samp_rate
        self.channel_list  = channel_list

        # Define blocks
        self.channelizer = lora.channelizer(in_samp_rate, out_samp_rate, center_freq, channel_list)
        self.decoder = lora.decoder(out_samp_rate, sf)
        self.set_threshold(threshold)

        # Messages
        self.message_port_register_hier_out('frames')

        # Connect blocks
        self.connect((self, 0), (self.channelizer, 0))
        self.connect((self.channelizer, 0), (self.decoder, 0))
        self.msg_connect((self.decoder, 'frames'), (self, 'frames'))
        self.msg_connect((self.decoder, 'control'), (self.channelizer, 'control'))

    def get_sf(self):
        return self.sf

    def set_sf(self, sf):
        self.sf = sf
        self.decoder.set_sf(self.sf)

    def get_center_freq(self):
        return self.center_freq

    def set_center_freq(self, center_freq):
        self.center_freq = center_freq
        self.channelizer.set_center_freq(self.center_freq)

    def get_threshold(self):
        return self.threshold

    def set_threshold(self, threshold):
        self.threshold = threshold
        self.decoder.set_abs_threshold(self.threshold)
