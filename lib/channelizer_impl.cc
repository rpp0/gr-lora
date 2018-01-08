/* -*- c++ -*- */
/*
 * Copyright 2017 Pieter Robyns.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "channelizer_impl.h"

namespace gr {
  namespace lora {

    channelizer::sptr
    channelizer::make(float samp_rate, float center_freq, std::vector<float> channel_list, uint32_t bandwidth, uint32_t decimation) {
      return gnuradio::get_initial_sptr
        (new channelizer_impl(samp_rate, center_freq, channel_list, bandwidth, decimation));
    }

    /*
     * The private constructor
     */
    channelizer_impl::channelizer_impl(float samp_rate, float center_freq, std::vector<float> channel_list, uint32_t bandwidth, uint32_t decimation)
      : gr::hier_block2("channelizer",
              gr::io_signature::make(1, 1, sizeof(gr_complex)),
              gr::io_signature::make(channel_list.size(), channel_list.size(), sizeof(gr_complex))),
        d_cfo(0.0)
    {
        d_lpf = gr::filter::firdes::low_pass(1.0, samp_rate, (bandwidth/2)+15000, 10000, gr::filter::firdes::WIN_HAMMING, 6.67);
        d_freq_offset = channel_list[0] - center_freq;
        d_xlating_fir_filter = gr::filter::freq_xlating_fir_filter_ccf::make(decimation, d_lpf, d_freq_offset, samp_rate);
        d_controller = gr::lora::controller::make((void*)this);
        //d_resampler = gr::filter::fractional_resampler_cc::make(0, (float)in_samp_rate / (float)out_samp_rate);
        //self.delay = delay(gr.sizeof_gr_complex, int((len(lpf)-1) / 2.0))

        //Create message ports
        message_port_register_hier_in(pmt::intern("control"));

        connect(self(), 0, d_xlating_fir_filter, 0);
        connect(d_xlating_fir_filter, 0, self(), 0);

        msg_connect(self(), pmt::intern("control"), d_controller, pmt::intern("control"));
    }

    /*
     * Our virtual destructor.
     */
    channelizer_impl::~channelizer_impl() {
    }

    void channelizer_impl::apply_cfo(float cfo) {
        d_cfo += cfo;
        d_xlating_fir_filter->set_center_freq(d_freq_offset + d_cfo);
    }


  } /* namespace lora */
} /* namespace gr */
