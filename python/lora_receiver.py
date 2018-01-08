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
import gnuradio
import lora

class lora_receiver(gr.hier_block2):
    """
    docstring for block lora_receiver
    """
    def __init__(self, samp_rate, center_freq, channel_list, bandwidth, sf, implicit, cr, crc, reduced_rate=False, conj=False, decimation=1, disable_channelization=False, disable_drift_correction=False):
        gr.hier_block2.__init__(self,
            "lora_receiver",  # Min, Max, gr.sizeof_<type>
            gr.io_signature(1, 1, gr.sizeof_gr_complex),  # Input signature
            gr.io_signature(0, 0, 0)) # Output signature

        # Parameters
        self.samp_rate     = samp_rate
        self.center_freq   = center_freq
        self.channel_list  = channel_list
        self.bandwidth     = bandwidth
        self.sf            = sf
        self.implicit      = implicit
        self.cr            = cr
        self.crc           = crc
        self.decimation    = decimation
        self.conj          = conj
        self.disable_channelization = disable_channelization
        self.disable_drift_correction = disable_drift_correction

        # Define blocks
        self.block_conj = gnuradio.blocks.conjugate_cc()
        self.channelizer = lora.channelizer(samp_rate, center_freq, channel_list, bandwidth, decimation)
        self.decoder = lora.decoder(samp_rate / decimation, bandwidth, sf, implicit, cr, crc, reduced_rate, disable_drift_correction)

        # Messages
        self.message_port_register_hier_out('frames')

        # Connect blocks
        if self.disable_channelization:
            self.resampler = gnuradio.filter.fractional_resampler_cc(0, float(decimation))
            self.connect((self, 0), (self.resampler, 0))
            self._connect_conj_block_if_enabled(self.resampler, self.decoder)
        else:
            self.connect((self, 0), (self.channelizer, 0))
            self._connect_conj_block_if_enabled(self.channelizer, self.decoder)
            self.msg_connect((self.decoder, 'control'), (self.channelizer, 'control'))

        self.msg_connect((self.decoder, 'frames'), (self, 'frames'))

    def _connect_conj_block_if_enabled(self, source, dest):
        if self.conj:
            self.connect((source, 0), (self.block_conj, 0))
            self.connect((self.block_conj, 0), (dest, 0))
        else:
            self.connect((source, 0), (dest, 0))

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
