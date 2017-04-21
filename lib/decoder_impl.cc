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

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/expj.h>
#include <liquid/liquid.h>
#include <numeric>
#include <algorithm>
#include "decoder_impl.h"
#include "tables.h"
#include "utilities.h"

//#define NO_TMP_WRITES 1   /// Debug output file write
//#define CFO_CORRECT   1   /// Correct shift fft estimation


//#undef NDEBUG            /// Debug printing
#include "dbugr.hpp"

namespace gr {
    namespace lora {

        decoder::sptr decoder::make(float samp_rate, int sf) {
            return gnuradio::get_initial_sptr
                   (new decoder_impl(samp_rate, sf));
        }

        /**
         * The private constructor
         */
        decoder_impl::decoder_impl(float samp_rate, uint8_t sf)
            : gr::sync_block("decoder",
                             gr::io_signature::make(1, -1, sizeof(gr_complex)),
                             gr::io_signature::make(0,  2, sizeof(float))) {
            this->d_state = gr::lora::DecoderState::DETECT;

            if (sf < 6 || sf > 13) {
                //throw std::invalid_argument("[LoRa Decoder] ERROR : Spreading factor should be between 6 and 12 (inclusive)!\n                       Other values are currently not supported.");
                std::cerr << "[LoRa Decoder] ERROR : Spreading factor should be between 6 and 12 (inclusive)!" << std::endl
                          << "                       Other values are currently not supported." << std::endl;
                exit(1);
            }

            // Set whitening sequence
            switch(sf) {
                case  6: this->d_whitening_sequence = gr::lora::prng_payload_sf12; break; // Using _sf6 results in worse accuracy...
                case  7: this->d_whitening_sequence = gr::lora::prng_payload_sf7;  break;
                case  8: this->d_whitening_sequence = gr::lora::prng_payload_sf8;  break;
                case  9: this->d_whitening_sequence = gr::lora::prng_payload_sf9;  break;
                case 10: this->d_whitening_sequence = gr::lora::prng_payload_sf10; break;
                case 11: this->d_whitening_sequence = gr::lora::prng_payload_sf11; break;
                case 12: this->d_whitening_sequence = gr::lora::prng_payload_sf12; break;
                default: this->d_whitening_sequence = gr::lora::prng_payload_sf7;  break;
            }

            if (sf == 6) {
                std::cerr << "[LoRa Decoder] WARNING : Spreading factor wrapped around to 12 due to incompatibility in hardware!" << std::endl;
                sf = 12;
            }

            #ifndef NDEBUG
                this->d_debug_samples.open("/tmp/grlora_debug", std::ios::out | std::ios::binary);
                this->d_debug.open("/tmp/grlora_debug_txt", std::ios::out);
            #endif

            this->d_bw                 = 125000u;
            this->d_cr                 = 4;
            this->d_samples_per_second = samp_rate;
            this->d_corr_decim_factor  = 8u; // samples_per_symbol / corr_decim_factor = correlation window. Also serves as preamble decimation factor
            this->d_payload_symbols    = 0;
            this->d_cfo_estimation     = 0.0f;
            this->d_dt                 = 1.0f / this->d_samples_per_second;

            this->d_sf                 = sf;  // Only affects PHY send
            this->d_bits_per_second    = (double)this->d_sf * (double)(1 + this->d_cr) / (1 << this->d_sf) * this->d_bw;
            this->d_symbols_per_second = (double)this->d_bw / (1 << this->d_sf);
            this->d_bits_per_symbol    = (uint32_t)(this->d_bits_per_second    / this->d_symbols_per_second);
            this->d_samples_per_symbol = (uint32_t)(this->d_samples_per_second / this->d_symbols_per_second);
            this->d_delay_after_sync   = this->d_samples_per_symbol / 4;
            this->d_number_of_bins     = (uint32_t)(1 << this->d_sf);
            this->d_number_of_bins_hdr = this->d_number_of_bins / 4;

            this->d_energy_threshold   = 0.01f;

            // Some preparations
            std::cout << "Bits per symbol: \t"      << this->d_bits_per_symbol    << std::endl;
            std::cout << "Bins per symbol: \t"      << this->d_number_of_bins     << std::endl;
            std::cout << "Header bins per symbol: " << this->d_number_of_bins_hdr << std::endl;
            std::cout << "Samples per symbol: \t"   << this->d_samples_per_symbol << std::endl;
            std::cout << "Decimation: \t\t"         << (this->d_samples_per_symbol / this->d_number_of_bins) << std::endl;
            //std::cout << "Magnitude threshold:\t"   << this->d_energy_threshold   << std::endl;

            this->build_ideal_chirps();

            this->set_output_multiple(2 * this->d_samples_per_symbol);
            this->d_fft.resize(this->d_number_of_bins);
            this->d_mult.resize(this->d_number_of_bins);
            this->d_q = fft_create_plan(this->d_number_of_bins, &this->d_mult[0], &this->d_fft[0], LIQUID_FFT_FORWARD, 0);

            // Decimation filter
            float g[DECIMATOR_FILTER_SIZE];
            liquid_firdes_rrcos(8, 1, 0.5f, 0.3f, g); // Filter for interpolating

            for (uint32_t i = 0u; i < DECIMATOR_FILTER_SIZE; i++) // Reverse it to get decimation filter
                this->d_decim_h[i] = g[DECIMATOR_FILTER_SIZE - i - 1];

            this->d_decim_factor = this->d_samples_per_symbol / this->d_number_of_bins;

            this->d_decim = firdecim_crcf_create(this->d_decim_factor, this->d_decim_h, DECIMATOR_FILTER_SIZE);

            // Register gnuradio ports
            this->message_port_register_out(pmt::mp("frames"));
            this->message_port_register_out(pmt::mp("debug"));

            // // Whitening empty file
            // DBGR_QUICK_TO_FILE("/tmp/whitening_out", false, g, -1, "");
        }

        /**
         * Our virtual destructor.
         */
        decoder_impl::~decoder_impl() {
            #ifndef NDEBUG
                if (this->d_debug_samples.is_open())
                    this->d_debug_samples.close();

                if (this->d_debug.is_open())
                    this->d_debug.close();
            #endif

            fft_destroy_plan(this->d_q);
            firdecim_crcf_destroy(this->d_decim);
        }

        void decoder_impl::build_ideal_chirps(void) {
            this->d_downchirp.resize(this->d_samples_per_symbol);
            this->d_upchirp.resize(this->d_samples_per_symbol);
            this->d_downchirp_ifreq.resize(this->d_samples_per_symbol);
            this->d_upchirp_ifreq.resize(this->d_samples_per_symbol);

            const double T       = -0.5 * this->d_bw * this->d_symbols_per_second;
            const double f0      = (this->d_bw / 2.0f);
            const double pre_dir = 2.0f * M_PI;
            double t;
            gr_complex cmx       = gr_complex(1.0f, 1.0f);

            for (uint32_t i = 0u; i < this->d_samples_per_symbol; i++) {
                // Width in number of samples = samples_per_symbol
                // See https://en.wikipedia.org/wiki/Chirp#Linear
                t = this->d_dt * i;
                this->d_downchirp[i] = cmx * gr_expj(pre_dir * t * (f0 + T * t));
                this->d_upchirp[i]   = cmx * gr_expj(pre_dir * t * (f0 + T * t) * -1.0f);
            }

            // Store instant. frequency
            this->instantaneous_frequency(&this->d_downchirp[0], &this->d_downchirp_ifreq[0], this->d_samples_per_symbol);
            this->instantaneous_frequency(&this->d_upchirp[0],   &this->d_upchirp_ifreq[0],   this->d_samples_per_symbol);

            samples_to_file("/tmp/downchirp", &this->d_downchirp[0], this->d_downchirp.size(), sizeof(gr_complex));
            samples_to_file("/tmp/upchirp",   &this->d_upchirp[0],   this->d_upchirp.size(),   sizeof(gr_complex));
        }

        void decoder_impl::samples_to_file(const std::string path, const gr_complex *v, uint32_t length, uint32_t elem_size) {
            #ifndef NO_TMP_WRITES
                std::ofstream out_file;
                out_file.open(path.c_str(), std::ios::out | std::ios::binary);

                //for(std::vector<gr_complex>::const_iterator it = v.begin(); it != v.end(); ++it) {
                for (uint32_t i = 0u; i < length; i++) {
                    out_file.write(reinterpret_cast<const char *>(&v[i]), elem_size);
                }

                out_file.close();
            #else
                (void) path;
                (void) v;
                (void) length;
                (void) elem_size;
            #endif
        }

        void decoder_impl::samples_debug(const gr_complex *v, uint32_t length) {
            #ifndef NDEBUG
                gr_complex start_indicator(0.0f, 32.0f);
                this->d_debug_samples.write(reinterpret_cast<const char *>(&start_indicator), sizeof(gr_complex));

                for (uint32_t i = 1; i < length; i++) {
                    this->d_debug_samples.write(reinterpret_cast<const char *>(&v[i]), sizeof(gr_complex));
                }
            #else
                (void) v;
                (void) length;
            #endif
        }

        bool decoder_impl::calc_energy_threshold(gr_complex *samples, uint32_t window_size, float threshold) {
            float result = 0.0f;

            for (uint32_t i = 0u; i < window_size; i++) {
                const float magn = std::abs(samples[i]);
                result += magn * magn;
            }

            result /= (float)window_size;

            #ifndef NDEBUG
                this->d_debug << "T: " << result << "\n";
            #endif

            return result > threshold;
        }

        inline void decoder_impl::instantaneous_frequency(const gr_complex *in_samples, float *out_ifreq, uint32_t window) {
            if (window < 2) {
                std::cerr << "[LoRa Decoder] WARNING : window size < 2 !" << std::endl;
                return;
            }

            /* instantaneous_phase */
            for (uint32_t i = 1; i < window; i++) {
                const float iphase_1 = std::arg(in_samples[i - 1]);
                      float iphase_2 = std::arg(in_samples[i]);

                // Unwrapped loops from liquid_unwrap_phase
                while ( (iphase_2 - iphase_1) >  M_PI ) iphase_2 -= 2.0f*M_PI;
                while ( (iphase_2 - iphase_1) < -M_PI ) iphase_2 += 2.0f*M_PI;

                out_ifreq[i - 1] = iphase_2 - iphase_1;
            }

            // Make sure there is no strong gradient if this value is accessed by mistake
            out_ifreq[window - 1] = out_ifreq[window - 2];
        }

        /**
         *  Currently unused.
         */
        inline void decoder_impl::instantaneous_phase(const gr_complex *in_samples, float *out_iphase, uint32_t window) {
            out_iphase[0] = std::arg(in_samples[0]);

            for (uint32_t i = 1; i < window; i++) {
                out_iphase[i] = std::arg(in_samples[i]);
                // = the same as atan2(imag(in_samples[i]),real(in_samples[i]));

                // Unwrapped loops from liquid_unwrap_phase
                while ( (out_iphase[i] - out_iphase[i-1]) >  M_PI ) out_iphase[i] -= 2.0f*M_PI;
                while ( (out_iphase[i] - out_iphase[i-1]) < -M_PI ) out_iphase[i] += 2.0f*M_PI;
            }
        }

        /**
         *  Currently unused.
         */
        float decoder_impl::cross_correlate(const gr_complex *samples_1, const gr_complex *samples_2, uint32_t window) {
            float result = 0.0f;

            for (uint32_t i = 0u; i < window; i++) {
                result += std::real(samples_1[i] * std::conj(samples_2[i]));
            }

            result /= (float)window;

            return result;
        }

        /**
         * Calculate normalized cross correlation of real values.
         * See https://en.wikipedia.org/wiki/Cross-correlation#Normalized_cross-correlation.
         */
        float decoder_impl::cross_correlate_ifreq(const float *samples, std::vector<float>& ideal_chirp, uint32_t from_idx, uint32_t to_idx) {
            float result = 0.0f;

            const float average   = std::accumulate(samples + from_idx    , samples + to_idx    , 0.0f) / (float)(to_idx - from_idx);
            const float chirp_avg = std::accumulate(&ideal_chirp[from_idx], &ideal_chirp[to_idx], 0.0f) / (float)(to_idx - from_idx);
            const float sd        =   this->stddev(samples + from_idx     , (to_idx - from_idx) , average)
                                    * this->stddev(&ideal_chirp[from_idx] , (to_idx - from_idx) , chirp_avg);

            for (uint32_t i = from_idx; i < to_idx; i++) {
                result += (samples[i] - average) * (ideal_chirp[i] - chirp_avg) / sd;
            }

            result /= (float)(to_idx - from_idx - 1u);

            return result;
        }

        float decoder_impl::detect_downchirp(const gr_complex *samples, uint32_t window) {
            float samples_ifreq[window];

            this->instantaneous_frequency(samples, samples_ifreq, window);
            return this->cross_correlate_ifreq(samples_ifreq, this->d_downchirp_ifreq, 0, window);
        }

        float decoder_impl::sliding_norm_cross_correlate_upchirp(const float *samples, uint32_t window, uint32_t slide, int32_t *index) {
            (void) slide;

            bool found_change = false;
            uint32_t max = 0u, min;
            const uint32_t coeff = 16u;

            for (uint32_t i = 0u; i < window - coeff - 1u; i += coeff / 2u) {
                if (samples[i] - samples[i + coeff]  > 0.2f) { // Goes down
                    max = i;
                    found_change = true;
                    break;
                }
            }

            if (!found_change) {
                return 0.0f;
            }

            max = std::max_element(samples + gr::lora::clamp((int)(max - coeff),  0, (int)window),
                                   samples + gr::lora::clamp(max +        coeff, 0u,      window)) - samples;
            min = std::min_element(samples + gr::lora::clamp(max +           1u, 0u,      window),
                                   samples + gr::lora::clamp(max +   2u * coeff, 0u,      window)) - samples;

            max += (min - max) / 2;
            *index = max;

            // Extra to allow falling edge on 99% of sample == Practically synced
            // Should improve detection by not discarding almost synced samples, but doesn't..
//            if ((max - coeff / 2) > (window * 0.98f)) {
////                printf("ALREADY SYNCED\n");
//                *index = 0;
//                return 0.99f;
//            }

            return this->cross_correlate_ifreq(samples, this->d_upchirp_ifreq, max, window);
        }

        /**
         *  Slide the given chirp perfectly on top of the ideal upchirp (phase shift).
         */
        int32_t decoder_impl::slide_phase_shift_upchirp_perfect(const float* samples, uint32_t window) {
            /// Perfect shift to ideal frequency
            const uint32_t t_low = window / 4u,
                           t_mid = window / 2u;

            // Average before compare
            const uint32_t coeff = 20u;
            float avg = std::accumulate(&samples[t_mid] - coeff / 2u, &samples[t_mid] + coeff / 2u, 0.0f) / coeff;

            uint32_t idx = std::lower_bound( this->d_upchirp_ifreq.begin() + t_low,
                                             this->d_upchirp_ifreq.begin() + t_mid,
                                             avg)
                           - this->d_upchirp_ifreq.begin();

            return (idx <= t_low || idx >= t_mid) ? -1 : t_mid - idx;
        }

        float decoder_impl::stddev(const float *values, uint32_t len, float mean) {
            float variance = 0.0f;

            for (uint32_t i = 0u; i < len; i++) {
                const float temp = values[i] - mean;
                variance += temp * temp;
            }

            variance /= (float)len;
            return std::sqrt(variance);
        }

        float decoder_impl::detect_upchirp(const gr_complex *samples, uint32_t window, uint32_t slide, int32_t *index) {
            float samples_ifreq[window];

            this->instantaneous_frequency(samples, samples_ifreq, window);

            return this->sliding_norm_cross_correlate_upchirp(samples_ifreq, window, slide, index);
        }

        /**
         *  Currently unused.
         */
        unsigned int decoder_impl::get_shift_fft(const gr_complex *samples) {
            float      fft_mag[this->d_number_of_bins];
            gr_complex mult_hf[this->d_samples_per_symbol];

            #ifdef CFO_CORRECT
                determine_cfo(&samples[0]);
                #ifndef NDEBUG
                    this->d_debug << "CFO: " << this->d_cfo_estimation << std::endl;
                #endif
                correct_cfo(&samples[0], this->d_samples_per_symbol);
            #endif

            samples_to_file("/tmp/data", &samples[0], this->d_samples_per_symbol, sizeof(gr_complex));

            // Multiply with ideal downchirp
            for (uint32_t i = 0u; i < this->d_samples_per_symbol; i++) {
                mult_hf[i] = std::conj(samples[i] * this->d_downchirp[i]);
            }

            samples_to_file("/tmp/mult", &mult_hf[0], this->d_samples_per_symbol, sizeof(gr_complex));

            // Perform decimation
            for (uint32_t i = 0u; i < this->d_number_of_bins; i++) {
                firdecim_crcf_execute(this->d_decim, &mult_hf[this->d_decim_factor * i], &d_mult[i]);
            }

            samples_to_file("/tmp/resampled", &this->d_mult[0], this->d_number_of_bins, sizeof(gr_complex));

            // Perform FFT
            fft_execute(this->d_q);

            // Get magnitude
            for (uint32_t i = 0u; i < this->d_number_of_bins; i++) {
                fft_mag[i] = std::abs(this->d_fft[i]);
            }

            samples_to_file("/tmp/fft", &this->d_fft[0], this->d_number_of_bins, sizeof(gr_complex));

            // Return argmax here
            return (std::max_element(fft_mag, fft_mag + this->d_number_of_bins) - fft_mag);
        }

        unsigned int decoder_impl::max_frequency_gradient_idx(gr_complex *samples) {
            float instantaneous_freq [this->d_samples_per_symbol];
            float        max_if_diff     = 0.0f; //2000.0f;
            unsigned int max_if_diff_idx = 0u;

            samples_to_file("/tmp/data", &samples[0], this->d_samples_per_symbol, sizeof(gr_complex));

            this->instantaneous_frequency(samples, instantaneous_freq, this->d_samples_per_symbol);

            const uint32_t osr = this->d_samples_per_symbol / this->d_number_of_bins;
            for (uint32_t i = 0u; i < this->d_number_of_bins - 1u; i++) {
                if (instantaneous_freq[osr * i] - instantaneous_freq[osr * (i + 1u)] > 0.2f) {
                    return i + 1u;
                }
            }

            return (instantaneous_freq[0u] - instantaneous_freq[osr])
                    > (instantaneous_freq[(this->d_number_of_bins - 1u) * osr] - instantaneous_freq[this->d_number_of_bins * osr])
                    ? 0u : this->d_number_of_bins;
        }

        bool decoder_impl::demodulate(gr_complex *samples, bool is_header) {
            uint32_t bin_idx = this->max_frequency_gradient_idx(samples);
            uint32_t bin_idx_test = 0u;

            // Header has additional redundancy
            if (is_header) {
                bin_idx      /= 4u;
                bin_idx_test /= 4u;
            }

            // Decode (actually gray encode) the bin to get the symbol value
            unsigned int word = /* LiquidDSP:: */ gray_encode(bin_idx);
            #ifndef NDEBUG
                this->d_debug << gr::lora::to_bin(word, is_header ? this->d_sf - 2 : this->d_sf) << " " << bin_idx  << std::endl;
            #endif
            this->d_words.push_back(word);

            // Look for 4+cr symbols and stop
            if (this->d_words.size() == (4u + this->d_cr)) {
                // Deinterleave
                this->deinterleave(is_header ? this->d_sf - 2 : this->d_sf);

                return true; // Signal that a block is ready for decoding
            }

            return false; // We need more words in order to decode a block
        }

        void decoder_impl::deinterleave(uint32_t ppm) {
            const unsigned int bits_per_word = this->d_words.size();

            if (bits_per_word > 8u) {
                // Not sure if this can ever occur. It would imply coding rate high than 4/8 e.g. 4/9.
                std::cerr << "[LoRa Decoder] WARNING : Deinterleaver: More than 8 bits per word. uint8_t will not be sufficient!\nBytes need to be stored in intermediate array and then packed into words_deinterleaved!" << std::endl;
            }

            std::deque<uint8_t> words_deinterleaved;
            uint32_t offset_start = ppm - 1u, offset_diag, i;
            uint8_t d;

            for (i = 0u; i < ppm; i++) {
                d = 0;
                offset_diag = offset_start;

                for (uint32_t j = 0u; j < bits_per_word; j++) {
                    const uint8_t  power       = 1u << j;
                    const uint32_t power_check = 1u << offset_diag;

                    if (this->d_words[j] & power_check) { // Mask triggers
                        d += power;
                    }

                    if (offset_diag) offset_diag--;
                    else             offset_diag = ppm - 1u;
                }

                offset_start--;
                words_deinterleaved.push_front(d);
            }

            #ifndef NDEBUG
                std::vector<uint8_t> wd(words_deinterleaved.begin(), words_deinterleaved.begin() + ppm-1);
                print_vector(this->d_debug, wd, "D", sizeof(uint8_t) * 8);
            #endif

            // Add to demodulated data
            this->d_demodulated.insert(this->d_demodulated.end(), words_deinterleaved.begin(), words_deinterleaved.end());

            // Cleanup
            this->d_words.clear();
        }

        int decoder_impl::decode(uint8_t *out_data, bool is_header) {
            const uint8_t shuffle_pattern[] = {7, 6, 3, 4, 2, 1, 0, 5};

            this->deshuffle(shuffle_pattern, is_header);
            this->dewhiten(is_header ? gr::lora::prng_header : this->d_whitening_sequence);
            this->hamming_decode(out_data);

            // Nibbles are reversed TODO why is this?
            this->nibble_reverse(out_data, this->d_payload_length);

            // Print result
            std::stringstream result;

            for (uint32_t i = 0u; i < this->d_payload_length; i++) {
                result << " " << std::hex << std::setw(2) << std::setfill('0') << (int)out_data[i];
            }

            if (!is_header) {
                this->d_data.insert(this->d_data.end(), out_data, out_data + this->d_payload_length);
                std::cout << result.str() << std::endl;

                pmt::pmt_t payload_blob = pmt::make_blob(&this->d_data[0],
                                                         sizeof(uint8_t) * (this->d_payload_length + 3));
                this->message_port_pub(pmt::mp("frames"), payload_blob);
            } else {
                this->d_data.insert(this->d_data.end(), out_data, out_data + 3);
                std::cout << result.str();
            }

            return 0;
        }

        void decoder_impl::deshuffle(const uint8_t *shuffle_pattern, bool is_header) {
            const uint32_t to_decode = is_header ? 5 : this->d_demodulated.size();
            const uint32_t len       = sizeof(shuffle_pattern) / sizeof(uint8_t);
            uint8_t original, result;

            for (uint32_t i = 0u; i < to_decode; i++) {
                original = this->d_demodulated[i];
                result   = 0;

                for (uint32_t j = 0u; j < len; j++) {
                    if (original & (1 << shuffle_pattern[j])) {
                        result |= 1 << j;
                    }
                }

                this->d_words_deshuffled.push_back(result);
            }

            #ifndef NDEBUG
                //print_vector(d_debug, d_words_deshuffled, "S", sizeof(uint8_t)*8);
                print_vector_raw(this->d_debug, this->d_words_deshuffled, sizeof(uint8_t) * 8);
                this->d_debug << std::endl;
            #endif

            // We're done with these words
            if (is_header){
                this->d_demodulated.erase(this->d_demodulated.begin(), this->d_demodulated.begin() + 5);
            } else {
                this->d_demodulated.clear();
            }
        }

        void decoder_impl::dewhiten(const uint8_t *prng) {
            uint32_t i, len = this->d_words_deshuffled.size();

            // // Whitening out
            // if (prng != gr::lora::prng_header)
                // DBGR_QUICK_TO_FILE("/tmp/whitening_out", true, this->d_words_deshuffled, len, "0x%02X,");

            for (i = 0u; i < len; i++) {
                uint8_t xor_b = this->d_words_deshuffled[i] ^ prng[i];

                // TODO: reverse bit order is performed here,
                //       but is probably due to mistake in whitening or interleaving
                xor_b = (xor_b & 0xF0) >> 4 | (xor_b & 0x0F) << 4;
                xor_b = (xor_b & 0xCC) >> 2 | (xor_b & 0x33) << 2;
                xor_b = (xor_b & 0xAA) >> 1 | (xor_b & 0x55) << 1;
                this->d_words_dewhitened.push_back(xor_b);
            }

            #ifndef NDEBUG
                print_vector(this->d_debug, this->d_words_dewhitened, "W", sizeof(uint8_t) * 8);
            #endif

            this->d_words_deshuffled.clear();
        }

        void decoder_impl::hamming_decode(uint8_t *out_data) {
            uint8_t data_indices[4] = {1, 2, 3, 5};

            switch(this->d_cr) {
                case 4: case 3:
                    gr::lora::hamming_decode_soft(&this->d_words_dewhitened[0], this->d_words_dewhitened.size(), out_data);
                    break;
                case 2: case 1: // TODO: Report parity error to the user
                    gr::lora::fec_extract_data_only(&this->d_words_dewhitened[0], this->d_words_dewhitened.size(), data_indices, 4, out_data);
                    break;
            }

            this->d_words_dewhitened.clear();

            /*
            fec_scheme fs = LIQUID_FEC_HAMMING84;
            unsigned int n = ceil(this->d_words_dewhitened.size() * 4.0f / (4.0f + d_cr));

            unsigned int k = fec_get_enc_msg_length(fs, n);
            fec hamming = fec_create(fs, NULL);

            fec_decode(hamming, n, &d_words_dewhitened[0], out_data);

            d_words_dewhitened.clear();
            fec_destroy(hamming);*/
        }

        void decoder_impl::nibble_reverse(uint8_t *out_data, uint32_t len) {
            for (uint32_t i = 0u; i < len; i++) {
                out_data[i] = ((out_data[i] & 0x0f) << 4) | ((out_data[i] & 0xf0) >> 4);
            }
        }

        /**
         *  Currently unused.
         */
        void decoder_impl::determine_cfo(const gr_complex *samples) {
            float instantaneous_phase[this->d_samples_per_symbol];
//            float instantaneous_freq [this->d_samples_per_symbol];
            double div = (double) this->d_samples_per_second / (2.0f * M_PI);

            // Determine instant phase
            for (uint32_t i = 0u; i < this->d_samples_per_symbol; i++) {
                instantaneous_phase[i] = arg(samples[i]);
            }

            liquid_unwrap_phase(instantaneous_phase, this->d_samples_per_symbol);

            // Determine instant freq
//            for (unsigned int i = 1; i < this->d_samples_per_symbol; i++) {
//                instantaneous_freq[i - 1] = (float)((instantaneous_phase[i] - instantaneous_phase[i - 1]) * div);
//            }

            float sum = 0.0f;

            for (uint32_t i = 1u; i < this->d_samples_per_symbol; i++) {
                sum += (float)((instantaneous_phase[i] - instantaneous_phase[i - 1u]) * div);
            }

            this->d_cfo_estimation = sum / (float)(this->d_samples_per_symbol - 1u);

            /*d_cfo_estimation = (*std::max_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1) + *std::min_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1)) / 2;*/
        }

        /**
         *  Currently unused.
         */
        void decoder_impl::correct_cfo(gr_complex *samples, uint32_t num_samples) {
            const float mul = 2.0f * M_PI * -this->d_cfo_estimation * this->d_dt;

            for (uint32_t i = 0u; i < num_samples; i++) {
                samples[i] *= gr_expj(mul * i);
            }
        }

        /**
         *  Currently unused.
         */
        int decoder_impl::find_preamble_start(gr_complex *samples) {
            for (uint32_t i = 0u; i < this->d_samples_per_symbol; i++) {
                if (!this->get_shift_fft(&samples[i]))
                    return i;
            }

            return -1;
        }

        int decoder_impl::find_preamble_start_fast(gr_complex *samples, uint32_t len) {
            (void) len;

            const uint32_t decimation = this->d_corr_decim_factor * 4u;
            const uint32_t decim_size = this->d_samples_per_symbol / decimation;

            // Absolute value
            for (uint32_t i = 1u; i < decimation - 1u; i++) {
                if (    std::abs(samples[ i       * decim_size]) > this->d_energy_threshold
                    &&  std::abs(samples[(i - 1u) * decim_size]) < std::abs(samples[i * decim_size])
                    &&  std::abs(samples[(i + 1u) * decim_size]) > std::abs(samples[i * decim_size])
                   ) {
                    return i * decim_size;
                }
            }
            
            return -1;
        }

        uint8_t decoder_impl::lookup_cr(uint8_t bytevalue) {
            switch (bytevalue & 0x0f) {
                case 0x01:  return 4;
                case 0x0f:  return 3;
                case 0x0d:  return 2;
                case 0x0b:  return 1;
                default:    return 4;
            }
        }

        void decoder_impl::msg_raw_chirp_debug(const gr_complex *raw_samples, uint32_t num_samples) {
            pmt::pmt_t chirp_blob = pmt::make_blob(raw_samples, sizeof(gr_complex) * num_samples);
            message_port_pub(pmt::mp("debug"), chirp_blob);
        }

        void decoder_impl::msg_lora_frame(const uint8_t *frame_bytes, uint32_t frame_len) {
            // ?? No implementation
        }

        int decoder_impl::work(int noutput_items,
                               gr_vector_const_void_star& input_items,
                               gr_vector_void_star&       output_items) {
            (void) noutput_items;
            (void) output_items;

            gr_complex *input     = (gr_complex *) input_items[0];
            gr_complex *raw_input = (gr_complex *) input_items[1];
//            float *out = (float *)output_items[0];

            switch (this->d_state) {
                case gr::lora::DecoderState::DETECT: {
                    int i = this->find_preamble_start_fast(&input[0], 2 * this->d_samples_per_symbol);

                    if (i != -1) {
                        // BUG: (2*d_samples_per_symbol - i) always bigger than d_samples_per_symbol
                        //      Thus c_window is always d_samples_per_symbol?
//                        uint32_t c_window = std::min(2 * this->d_samples_per_symbol - i,
//                                                     this->d_samples_per_symbol);
                        uint32_t c_window = this->d_samples_per_symbol;

                        int32_t index_correction = 0;

                        float c = this->detect_upchirp(&input[i],
                                                       c_window,
                                                       this->d_samples_per_symbol / this->d_corr_decim_factor,
                                                       &index_correction);

                        if (c > 0.8f) {
                            #ifndef NDEBUG
                                this->d_debug << "Cu: " << c << std::endl;
                            #endif
                            this->samples_to_file("/tmp/detectb", &input[i],                    this->d_samples_per_symbol, sizeof(gr_complex));
                            this->samples_to_file("/tmp/detect",  &input[i + index_correction], this->d_samples_per_symbol, sizeof(gr_complex));
                            this->d_corr_fails = 0u;
                            this->d_state = gr::lora::DecoderState::SYNC;
                            this->consume_each(i + index_correction);
                            break;
                        }

                        // Consume just 1 symbol after preamble to have more chances to sync later
                        this->consume_each(i + this->d_samples_per_symbol);
                    } else {
                        // Consume 2 symbols (usual) to skip noise faster before preamble has been found
                        this->consume_each(2 * this->d_samples_per_symbol);
                    }
                    break;
                }

                case gr::lora::DecoderState::SYNC: {
                    double c = this->detect_downchirp(&input[0], this->d_samples_per_symbol);
                    #ifndef NDEBUG
                        this->d_debug << "Cd: " << c << std::endl;
                    #endif

                    if (c > 0.98f) {
                        #ifndef NDEBUG
                            this->d_debug << "SYNC: " << c << std::endl;
                        #endif
                        // Debug stuff
                        this->samples_to_file("/tmp/sync", &input[0], this->d_samples_per_symbol, sizeof(gr_complex));

                        // printf("---------------------- SYNC!\n");

                        this->d_state = gr::lora::DecoderState::PAUSE;
                    } else {
                        this->d_corr_fails++;

                        if (this->d_corr_fails > 32) {
                            this->d_state = gr::lora::DecoderState::DETECT;
                            #ifndef NDEBUG
                                this->d_debug << "Lost sync" << std::endl;
                            #endif
                        }
                    }

                    this->consume_each(this->d_samples_per_symbol);
                    break;
                }

                case gr::lora::DecoderState::PAUSE: {
                    this->d_state = gr::lora::DecoderState::DECODE_HEADER;
                    //samples_debug(input, d_samples_per_symbol + d_delay_after_sync);
                    this->consume_each(this->d_samples_per_symbol + this->d_delay_after_sync);
                    break;
                }

                case gr::lora::DecoderState::DECODE_HEADER: {
                    this->d_cr = 4;

                    if (this->demodulate(input, true)) {
                        uint8_t decoded[3];
                        // TODO: A bit messy. I think it's better to make an internal decoded std::vector
                        this->d_payload_length  = 3u;

                        this->decode(decoded, true);

                        this->nibble_reverse(decoded, 1); // TODO: Why? Endianess?
                        this->d_payload_length  = decoded[0];
                        this->d_cr              = this->lookup_cr(decoded[1]);

                        const int symbols_per_block = this->d_cr + 4;
                        const float bits_needed     = float(this->d_payload_length) * 8.0f + 16.0f;
                        const float symbols_needed  = bits_needed * (symbols_per_block / 4.0f) / float(this->d_sf);
                        const int blocks_needed     = (int)std::ceil(symbols_needed / symbols_per_block);
                        this->d_payload_symbols     = blocks_needed * symbols_per_block;

                        #ifndef NDEBUG
                            this->d_debug << "LEN: " << this->d_payload_length << " (" << this->d_payload_symbols << " symbols)" << std::endl;
                        #endif

                        this->d_state = gr::lora::DecoderState::DECODE_PAYLOAD;
                    }
                    
                    this->msg_raw_chirp_debug(raw_input, this->d_samples_per_symbol);
                    //samples_debug(input, d_samples_per_symbol);
                    this->consume_each(this->d_samples_per_symbol);
                    break;
                }

                case gr::lora::DecoderState::DECODE_PAYLOAD: {
                    //**************************************************************************
                    // Failsafe if decoding length reaches end of actual data == noise reached?
                    if (std::abs(input[0]) < this->d_energy_threshold) {
//                        printf("\n*** Decode payload reached end of data! (payload length in HDR is wrong)\n");
                        this->d_payload_symbols = 0;
                    }
                    //**************************************************************************

                    if (this->demodulate(input, false)) {
                        this->d_payload_symbols -= (4 + this->d_cr);

                        if (this->d_payload_symbols <= 0) {
                            uint8_t decoded[this->d_payload_length] = { 0 };

                            this->decode(decoded, false);

                            this->d_state = gr::lora::DecoderState::DETECT;
                            this->d_data.clear();
                        }
                    }

                    this->msg_raw_chirp_debug(raw_input, this->d_samples_per_symbol);
                    //samples_debug(input, d_samples_per_symbol);
                    this->consume_each(this->d_samples_per_symbol);
                    break;
                }

                case gr::lora::DecoderState::STOP: {
                    this->consume_each(this->d_samples_per_symbol);
                    break;
                }

                default: {
                    std::cerr << "[LoRa Decoder] WARNING : No state! Shouldn't happen\n";
                    break;
                }
            }

            // Tell runtime system how many output items we produced.
            return 0;
        }

        void decoder_impl::set_sf(uint8_t sf) {
            (void) sf;
            std::cerr << "[LoRa Decoder] WARNING : Setting the spreading factor during execution is currently not supported." << std::endl
                      << "Nothing set, kept SF of " << this->d_sf << "." << std::endl;
        }

        void decoder_impl::set_samp_rate(float samp_rate) {
            (void) samp_rate;
            std::cerr << "[LoRa Decoder] WARNING : Setting the sample rate during execution is currently not supported." << std::endl
                      << "Nothing set, kept SR of " << this->d_samples_per_second << "." << std::endl;
        }

        void decoder_impl::set_abs_threshold(float threshold) {
            this->d_energy_threshold = gr::lora::clamp(threshold, 0.0f, 20.0f);
        }

    } /* namespace lora */
} /* namespace gr */
