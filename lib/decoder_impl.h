/* -*- c++ -*- */
/*
 * Copyright 2016 Pieter Robyns.
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

#ifndef INCLUDED_LORA_DECODER_IMPL_H
#define INCLUDED_LORA_DECODER_IMPL_H

#include <lora/decoder.h>
#include <list>
#include <string>
#include <vector>
#include <fstream>

namespace gr {
  namespace lora {

    typedef enum decoder_state {
        DETECT,
        SYNC,
        PAUSE,
        DECODE_HEADER,
        DECODE_PAYLOAD,
        STOP
    } decoder_state;

    class decoder_impl : public decoder
    {
     private:
        decoder_state d_state;
        std::vector<gr_complex> d_downchirp;
        std::vector<gr_complex> d_downchirp_fft;
        std::vector<gr_complex> d_fft;
        std::vector<gr_complex> d_mult;
        int32_t d_finetune;
        uint8_t d_sf;
        uint32_t d_bw;
        uint8_t d_cr;
        double d_bits_per_second;
        uint32_t d_samples_per_second;
        double d_symbols_per_second;
        uint32_t d_bits_per_symbol;
        uint32_t d_samples_per_symbol;
        uint32_t d_number_of_bins;
        uint32_t d_number_of_bins_hdr;
        uint32_t d_compression;
        uint32_t d_payload_symbols;
        uint32_t d_payload_length;
        uint32_t d_corr_fails;
        std::vector<unsigned int> d_words;
        std::vector<uint8_t> d_demodulated;
        std::vector<uint8_t> d_words_deshuffled;
        std::vector<uint8_t> d_words_dewhitened;
        std::vector<uint8_t> d_data;
        std::ofstream d_debug_samples;
        std::ofstream d_debug;
        fftplan d_q;

        bool calc_energy_threshold(gr_complex* samples, int window_size, float threshold);
        void build_ideal_downchirp(void);
        void samples_to_file(const std::string path, const gr_complex* v, int length, int elem_size);
        void samples_debug(const gr_complex* v, int length);
        double freq_cross_correlate(const gr_complex *samples_1, const gr_complex *samples_2, int window);
        int sync_fft(const gr_complex* samples);
        unsigned int max_frequency_gradient_idx(gr_complex* samples);
        bool demodulate(gr_complex* samples, bool is_header);
        void deinterleave(int ppm);
        int decode(uint8_t* out_data, bool is_header);
        void deshuffle(const uint8_t* shuffle_pattern);
        void dewhiten(const uint8_t* prng);
        void hamming_decode(uint8_t* out_data);
        void nibble_reverse(uint8_t* out_data, int len);
        inline void phase(gr_complex* in_samples, float* out_phase, int window);


     public:
        decoder_impl(int finetune);
        ~decoder_impl();

        // Where all the action really happens
        int work(int noutput_items,
            gr_vector_const_void_star &input_items,
            gr_vector_void_star &output_items);

        // GRC interfaces
        virtual void set_finetune(int32_t finetune);
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DECODER_IMPL_H */
