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


#ifndef INCLUDED_LORA_CHANNELIZER_IMPL_H
#define INCLUDED_LORA_CHANNELIZER_IMPL_H

#include <lora/channelizer.h>
#include <lora/controller.h>
#include <gnuradio/filter/freq_xlating_fir_filter.h>
#include <gnuradio/filter/mmse_resampler_cc.h>
#include <gnuradio/filter/firdes.h>

namespace gr {
  namespace lora {
    class channelizer_impl : public channelizer {
     private:
         gr::filter::freq_xlating_fir_filter_ccf::sptr d_xlating_fir_filter;
         //gr::filter::fractional_resampler_cc::sptr d_resampler;
         std::vector<float> d_lpf;
         float d_cfo;
         uint32_t d_freq_offset;
         gr::lora::controller::sptr d_controller;

     public:
      channelizer_impl(float samp_rate, float center_freq, std::vector<float> channel_list, uint32_t bandwidth, uint32_t decimation);
      ~channelizer_impl();
      void apply_cfo(float cfo);

      // Where all the action really happens
    };
  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_CHANNELIZER_IMPL_H */
