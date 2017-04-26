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

#include <liquid/liquid.h>
#include "lora/decoder.h"
#include <list>
#include <string>
#include <vector>
#include <deque>
#include <fstream>

#define DECIMATOR_FILTER_SIZE (2*8*1 + 1) // 2*decim_factor*delay+1

namespace gr {
    namespace lora {

        enum class DecoderState {
            DETECT,
            SYNC,
            PAUSE,
            DECODE_HEADER,
            DECODE_PAYLOAD,
            STOP
        };

        class decoder_impl : public decoder {
            private:
                DecoderState            d_state;

                /// using std::complex = gr_complex
                std::vector<gr_complex> d_downchirp;
                std::vector<float>      d_downchirp_ifreq;

                std::vector<gr_complex> d_upchirp;
                std::vector<float>      d_upchirp_ifreq;

                std::vector<gr_complex> d_fft;
                std::vector<gr_complex> d_mult;

                uint8_t        d_sf;
                uint32_t       d_bw;
                uint8_t        d_cr;
                double         d_bits_per_second;
                uint32_t       d_delay_after_sync;
                uint32_t       d_samples_per_second;
                double         d_symbols_per_second;
                uint32_t       d_bits_per_symbol;
                uint32_t       d_samples_per_symbol;
                uint32_t       d_number_of_bins;
                uint32_t       d_number_of_bins_hdr;
                uint32_t       d_compression;
                 int32_t       d_payload_symbols;
                uint32_t       d_payload_length;
                uint32_t       d_corr_fails;
                float          d_energy_threshold;
                const uint8_t *d_whitening_sequence;

                std::vector<unsigned int> d_words;
                std::vector<uint8_t>      d_demodulated;
                std::vector<uint8_t>      d_words_deshuffled;
                std::vector<uint8_t>      d_words_dewhitened;
                std::vector<uint8_t>      d_data;

                std::ofstream d_debug_samples;
                std::ofstream d_debug;

                fftplan d_q;

                float         d_decim_h[DECIMATOR_FILTER_SIZE];
                uint32_t      d_corr_decim_factor;
                int           d_decim_factor;
                firdecim_crcf d_decim = nullptr;
                float         d_cfo_estimation;
                int           d_cfo_step;
                double        d_dt;

                bool    calc_energy_threshold(const gr_complex *samples, const uint32_t window_size, const float threshold);
                void    build_ideal_chirps(void);
                void    samples_to_file(const std::string path, const gr_complex *v, const uint32_t length, const uint32_t elem_size);
                void    samples_debug(const gr_complex *v, const uint32_t length);
                float   sliding_norm_cross_correlate_upchirp(const float *samples, const uint32_t window, const uint32_t slide, int32_t *index);
                float   detect_downchirp(const gr_complex *samples, const uint32_t window);
                float   detect_upchirp(const gr_complex *samples_1, const uint32_t window, const uint32_t slide, int32_t *index);
                float   cross_correlate(const gr_complex *samples_1, const gr_complex *samples_2, const uint32_t window);
                float   cross_correlate_ifreq(const float *samples, const std::vector<float>& ideal_chirp, const uint32_t from_idx, const uint32_t to_idx);
                int32_t slide_phase_shift_upchirp_perfect(const float* samples, const uint32_t window);

                unsigned int get_shift_fft(const gr_complex *samples);

                void    determine_cfo(const gr_complex *samples);
                void    correct_cfo(gr_complex *samples, const uint32_t num_samples);
                int     find_preamble_start(const gr_complex *samples);
                int     find_preamble_start_fast(const gr_complex *samples, const uint32_t len);

                unsigned int max_frequency_gradient_idx(const gr_complex *samples);

                bool    demodulate(const gr_complex *samples, const bool is_header);
                void    deinterleave(const uint32_t ppm);
                int     decode(uint8_t *out_data, const bool is_header);
                void    deshuffle(const uint8_t *shuffle_pattern, const bool is_header);
                void    dewhiten(const uint8_t *prng);
                void    hamming_decode(uint8_t *out_data);
                void    nibble_reverse(uint8_t *out_data, const uint32_t len);
                float   stddev(const float *values, const uint32_t len, const float mean);

                inline void instantaneous_phase(const gr_complex *in_samples, float *out_iphase, const uint32_t window);
                inline void instantaneous_frequency(const gr_complex *in_samples, float *out_ifreq, const uint32_t window);

                uint8_t lookup_cr(const uint8_t bytevalue);
                void    msg_raw_chirp_debug(const gr_complex *raw_samples, const uint32_t num_samples);
                void    msg_lora_frame(const uint8_t *frame_bytes, const uint32_t frame_len);

            public:
                decoder_impl(float samp_rate, uint8_t sf);
                ~decoder_impl();

                /// Where all the action really happens
                int work(int noutput_items,
                         gr_vector_const_void_star& input_items,
                         gr_vector_void_star& output_items);

                /// GRC interfaces
                virtual void set_sf(const uint8_t sf);
                virtual void set_samp_rate(const float samp_rate);
                virtual void set_abs_threshold(const float threshold);
        };
    } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DECODER_IMPL_H */
