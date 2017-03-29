#!/usr/bin/env python2
# -*- coding: utf-8 -*-
##################################################
# GNU Radio Python Flow Graph
# Title: Lora Receive File
# Generated: Tue Mar 28 16:46:38 2017
##################################################

if __name__ == '__main__':
    import ctypes
    import sys
    if sys.platform.startswith('linux'):
        try:
            x11 = ctypes.cdll.LoadLibrary('libX11.so')
            x11.XInitThreads()
        except:
            print "Warning: failed to XInitThreads()"

from gnuradio import blocks
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import wxgui
from gnuradio.eng_option import eng_option
from gnuradio.fft import window
from gnuradio.filter import firdes
from gnuradio.wxgui import fftsink2
from gnuradio.wxgui import forms
from grc_gnuradio import wxgui as grc_wxgui
from optparse import OptionParser
import lora
import wx


class lora_receive_file(grc_wxgui.top_block_gui):

    def __init__(self):
        grc_wxgui.top_block_gui.__init__(self, title="Lora Receive File")

        ##################################################
        # Variables
        ##################################################
        self.target_freq = target_freq = 868.1e6
        self.sf = sf = 7
        self.samp_rate = samp_rate = 10e6
        self.capture_freq = capture_freq = 866.0e6
        self.bw = bw = 125e3
        self.symbols_per_sec = symbols_per_sec = bw / (2**sf)
        self.offset = offset = -(capture_freq - target_freq)
        self.firdes_tap = firdes_tap = firdes.low_pass(1, samp_rate, bw, 10000, firdes.WIN_HAMMING, 6.67)
        self.finetune = finetune = -95
        self.bitrate = bitrate = sf * (1 / (2**sf / bw))

        ##################################################
        # Blocks
        ##################################################
        self.wxgui_fftsink2_1 = fftsink2.fft_sink_c(
        	self.GetWin(),
        	baseband_freq=capture_freq,
        	y_per_div=10,
        	y_divs=10,
        	ref_level=0,
        	ref_scale=2.0,
        	sample_rate=samp_rate,
        	fft_size=1024,
        	fft_rate=15,
        	average=False,
        	avg_alpha=None,
        	title='FFT Plot',
        	peak_hold=False,
        )
        self.Add(self.wxgui_fftsink2_1.win)
        self.lora_lora_receiver_0 = lora.lora_receiver(samp_rate, capture_freq, offset, 7, 1000000)
        _finetune_sizer = wx.BoxSizer(wx.VERTICAL)
        self._finetune_text_box = forms.text_box(
        	parent=self.GetWin(),
        	sizer=_finetune_sizer,
        	value=self.finetune,
        	callback=self.set_finetune,
        	label='finetune',
        	converter=forms.int_converter(),
        	proportion=0,
        )
        self._finetune_slider = forms.slider(
        	parent=self.GetWin(),
        	sizer=_finetune_sizer,
        	value=self.finetune,
        	callback=self.set_finetune,
        	minimum=-150,
        	maximum=150,
        	num_steps=300,
        	style=wx.SL_HORIZONTAL,
        	cast=int,
        	proportion=1,
        )
        self.Add(_finetune_sizer)
        self.blocks_throttle_0 = blocks.throttle(gr.sizeof_gr_complex*1, samp_rate,True)
        self.blocks_file_source_0 = blocks.file_source(gr.sizeof_gr_complex*1, 'counting_cr4_sf7.cfile', True)

        ##################################################
        # Connections
        ##################################################
        self.connect((self.blocks_file_source_0, 0), (self.blocks_throttle_0, 0))    
        self.connect((self.blocks_throttle_0, 0), (self.lora_lora_receiver_0, 0))    
        self.connect((self.blocks_throttle_0, 0), (self.wxgui_fftsink2_1, 0))    

    def get_target_freq(self):
        return self.target_freq

    def set_target_freq(self, target_freq):
        self.target_freq = target_freq
        self.set_offset(-(self.capture_freq - self.target_freq))

    def get_sf(self):
        return self.sf

    def set_sf(self, sf):
        self.sf = sf
        self.set_symbols_per_sec(self.bw / (2**self.sf))
        self.set_bitrate(self.sf * (1 / (2**self.sf / self.bw)))

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.wxgui_fftsink2_1.set_sample_rate(self.samp_rate)
        self.set_firdes_tap(firdes.low_pass(1, self.samp_rate, self.bw, 10000, firdes.WIN_HAMMING, 6.67))
        self.blocks_throttle_0.set_sample_rate(self.samp_rate)

    def get_capture_freq(self):
        return self.capture_freq

    def set_capture_freq(self, capture_freq):
        self.capture_freq = capture_freq
        self.set_offset(-(self.capture_freq - self.target_freq))
        self.wxgui_fftsink2_1.set_baseband_freq(self.capture_freq)

    def get_bw(self):
        return self.bw

    def set_bw(self, bw):
        self.bw = bw
        self.set_symbols_per_sec(self.bw / (2**self.sf))
        self.set_firdes_tap(firdes.low_pass(1, self.samp_rate, self.bw, 10000, firdes.WIN_HAMMING, 6.67))
        self.set_bitrate(self.sf * (1 / (2**self.sf / self.bw)))

    def get_symbols_per_sec(self):
        return self.symbols_per_sec

    def set_symbols_per_sec(self, symbols_per_sec):
        self.symbols_per_sec = symbols_per_sec

    def get_offset(self):
        return self.offset

    def set_offset(self, offset):
        self.offset = offset
        self.lora_lora_receiver_0.set_offset(self.offset)

    def get_firdes_tap(self):
        return self.firdes_tap

    def set_firdes_tap(self, firdes_tap):
        self.firdes_tap = firdes_tap

    def get_finetune(self):
        return self.finetune

    def set_finetune(self, finetune):
        self.finetune = finetune
        self._finetune_slider.set_value(self.finetune)
        self._finetune_text_box.set_value(self.finetune)

    def get_bitrate(self):
        return self.bitrate

    def set_bitrate(self, bitrate):
        self.bitrate = bitrate


def main(top_block_cls=lora_receive_file, options=None):

    tb = top_block_cls()
    tb.Start(True)
    tb.Wait()


if __name__ == '__main__':
    main()
