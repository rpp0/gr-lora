/* -*- c++ -*- */
/*
 * Copyright 2018 Pieter Robyns.
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

#ifndef INCLUDED_LORA_ENCODER_IMPL_H
#define INCLUDED_LORA_ENCODER_IMPL_H

#include <liquid/liquid.h>
#include <lora/encoder.h>
#include <lora/loraconf.h>
#include <unordered_map>

namespace gr {
  namespace lora {

    class encoder_impl : public encoder {
     private:
         gr::thread::mutex d_mutex;
         std::vector<pmt::pmt_t> d_packets;
         std::unordered_map<uint8_t, std::vector<gr_complex> > d_chirps;
         fec d_h48_fec;
         uint32_t d_samples_per_second;
         uint32_t d_bw;
         double d_dt;
         uint16_t d_num_preamble_symbols;
         std::vector<gr_complex> d_sample_buffer;

     public:
      encoder_impl();
      ~encoder_impl();

      int work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
      void handle_loratap(pmt::pmt_t msg);
      bool parse_packet_conf(loraconf_t& conf, uint8_t* packet, uint32_t packet_len);
      void transmit_packet(loraconf_t& conf, uint8_t* packet);
      void shuffle(uint8_t *data, uint32_t data_len, const uint8_t *shuffle_pattern);
      void interleave(uint16_t *symbols, uint8_t* data, uint32_t data_len, uint8_t sf, uint8_t cr);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_ENCODER_IMPL_H */
