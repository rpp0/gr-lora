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
#include "decoder_impl.h"
#include "tables.h"
#include "utilities.h"

//#define NO_TMP_WRITES 1   /// Debug output file write
//#define CFO_CORRECT   1   /// Correct shift fft estimation


#undef NDEBUG            /// Debug printing

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
                std::cerr << "[LoRa Decoder] ERROR : Spreading factor should be between 6 and 12 (inclusive)!\n                       Other values are currently not supported." << std::endl;
                exit(1);
            }

            #ifndef NDEBUG
                this->d_debug_samples.open("/tmp/grlora_debug", std::ios::out | std::ios::binary);
                this->d_debug.open("/tmp/grlora_debug_txt", std::ios::out);
            #endif

            this->d_bw                 = 125000;
            this->d_cr                 = 4;
            this->d_samples_per_second = samp_rate;
            this->d_corr_decim_factor  = 8; // samples_per_symbol / corr_decim_factor = correlation window. Also serves as preamble decimation factor
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

            // Some preparations
            std::cout << "Bits per symbol: \t"      << this->d_bits_per_symbol    << std::endl;
            std::cout << "Bins per symbol: \t"      << this->d_number_of_bins     << std::endl;
            std::cout << "Header bins per symbol: " << this->d_number_of_bins_hdr << std::endl;
            std::cout << "Samples per symbol: \t"   << this->d_samples_per_symbol << std::endl;
            std::cout << "Decimation: \t\t"         << (this->d_samples_per_symbol / this->d_number_of_bins) << std::endl;

            this->build_ideal_chirps();

            this->set_output_multiple(2 * this->d_samples_per_symbol);
            this->d_fft.resize(this->d_number_of_bins);
            this->d_mult.resize(this->d_number_of_bins);
            this->d_q = fft_create_plan(this->d_number_of_bins, &this->d_mult[0], &this->d_fft[0], LIQUID_FFT_FORWARD, 0);

            // Decimation filter
            float g[DECIMATOR_FILTER_SIZE];
            liquid_firdes_rrcos(8, 1, 0.5f, 0.3f, g); // Filter for interpolating

            for (uint32_t i = 0; i < DECIMATOR_FILTER_SIZE; i++) // Reverse it to get decimation filter
                this->d_decim_h[i] = g[DECIMATOR_FILTER_SIZE - i - 1];

            this->d_decim_factor = this->d_samples_per_symbol / this->d_number_of_bins;

            this->d_decim = firdecim_crcf_create(this->d_decim_factor, this->d_decim_h, DECIMATOR_FILTER_SIZE);

            // Register gnuradio ports
            this->message_port_register_out(pmt::mp("frames"));
            this->message_port_register_out(pmt::mp("debug"));
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
            this->d_downchirp       .resize(this->d_samples_per_symbol);
            this->d_upchirp         .resize(this->d_samples_per_symbol);
            this->d_downchirp_ifreq .resize(this->d_samples_per_symbol);
            this->d_upchirp_ifreq   .resize(this->d_samples_per_symbol);

            const double T       = -0.5 * this->d_bw * this->d_symbols_per_second;
            const double f0      = (this->d_bw / 2.0f);
            const double pre_dir = 2.0f * M_PI;
            double t;
            gr_complex cmx       = gr_complex(1.0f, 1.0f);

            for (uint32_t i = 0; i < this->d_samples_per_symbol; i++) {
                // Width in number of samples = samples_per_symbol
                // See https://en.wikipedia.org/wiki/Chirp#Linear
                t = this->d_dt * i;
                this->d_downchirp[i] = cmx * gr_expj(pre_dir * t * (f0 + T * t));
                this->d_upchirp[i]   = cmx * gr_expj(pre_dir * t * (f0 + T * t) * -1.0f);
            }

            // Store instant. frequency
            instantaneous_frequency(&this->d_downchirp[0], &this->d_downchirp_ifreq[0], this->d_samples_per_symbol);
            instantaneous_frequency(&this->d_upchirp[0],   &this->d_upchirp_ifreq[0],   this->d_samples_per_symbol);

            samples_to_file("/tmp/downchirp", &this->d_downchirp[0], this->d_downchirp.size(), sizeof(gr_complex));
            samples_to_file("/tmp/upchirp",   &this->d_upchirp[0],   this->d_upchirp.size(),   sizeof(gr_complex));
        }

        void decoder_impl::samples_to_file(const std::string path, const gr_complex *v, uint32_t length, uint32_t elem_size) {
            #ifndef NO_TMP_WRITES
                std::ofstream out_file;
                out_file.open(path.c_str(), std::ios::out | std::ios::binary);

                //for(std::vector<gr_complex>::const_iterator it = v.begin(); it != v.end(); ++it) {
                for (uint32_t i = 0; i < length; i++) {
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

        bool decoder_impl::calc_energy_threshold(gr_complex *samples, int window_size, float threshold) {
            float result = 0.0f;

            for (int i = 0; i < window_size; i++) {
                float magn = abs(samples[i]);
                result += magn * magn;
            }

            result /= (float)window_size;

            #ifndef NDEBUG
                this->d_debug << "T: " << result << "\n";
            #endif

            return result > threshold;
        }

        void decoder_impl::instantaneous_frequency(const gr_complex *in_samples, float *out_ifreq, uint32_t window) {
            float iphase[window];

            if (window < 2) {
                // TODO: throw warning here
                std::cerr << "LoRa Decoder Warning: window size < 2 !" << std::endl;
                return;
            }

            this->instantaneous_phase(in_samples, iphase, window);

            // Instant freq
            for (uint32_t i = 1; i < window; i++) {
                out_ifreq[i - 1] = iphase[i] - iphase[i - 1];
            }

            // Make sure there is no strong gradient if this value is accessed by mistake
            out_ifreq[window - 1] = out_ifreq[window - 2];
        }

        inline void decoder_impl::instantaneous_phase(const gr_complex *in_samples, float *out_iphase, uint32_t window) {
            for (uint32_t i = 0; i < window; i++) {
                out_iphase[i] = arg(in_samples[i]);
                // = the same as atan2(imag(in_samples[i]),real(in_samples[i]));
            }

            liquid_unwrap_phase(out_iphase, window);
        }

        float decoder_impl::cross_correlate(const gr_complex *samples_1, const gr_complex *samples_2, int window) {
            float result = 0.0f;

            for (int i = 0; i < window; i++) {
                result += real(samples_1[i] * conj(samples_2[i]));
            }

            result /= (float)window;

            return result;
        }

        float decoder_impl::detect_downchirp(const gr_complex *samples, uint32_t window) {
            float samples_ifreq[window];

            instantaneous_frequency(samples, samples_ifreq, window);
            return norm_cross_correlate(samples_ifreq, &this->d_downchirp_ifreq[0], window);
        }

        /**
         * Calculate normalized cross correlation of real values.
         * See https://en.wikipedia.org/wiki/Cross-correlation#Normalized_cross-correlation.
         */
        float decoder_impl::norm_cross_correlate(const float *samples_1, const float *samples_2, uint32_t window) {
            float result = 0.0f;

            float average_1 = std::accumulate(samples_1, samples_1 + window, 0.0f) / window;
            float average_2 = std::accumulate(samples_2, samples_2 + window, 0.0f) / window;
            float sd_1      = stddev(samples_1, window, average_1);
            float sd_2      = stddev(samples_2, window, average_2);

            for (uint32_t i = 0; i < window - 1; i++) {
                result += (samples_1[i] - average_1) * (samples_2[i] - average_2)
                          / (sd_1 * sd_2);
            }

            result /= (float)(window - 1);

            return result;
        }

        float decoder_impl::sliding_norm_cross_correlate(const float *samples_1, const float *samples_2, uint32_t window, uint32_t slide, int32_t *index) {
            float correlations[slide * 2];
            float samples_1_padded[window + slide * 2] = { 0.0f };

            double average_1 = std::accumulate(samples_1, samples_1 + window, 0.0) / window;
            double average_2 = std::accumulate(samples_2, samples_2 + window, 0.0) / window;
            double sd_1      = stddev(samples_1, window, average_1);
            double sd_2      = stddev(samples_2, window, average_2);

            uint32_t i, j;
            float result;

            // Create padding on both sides of the samples
            for (i = 0; i < window; i++) {
                samples_1_padded[i + slide - 1] = samples_1[i];
            }

            // Slide and correlate
            for (i = 0; i < 2 * slide; i++) {
                result = 0.0f;

                for (j = 0; j < window; j++) {
                    result += (samples_1_padded[i + j] - average_1) * (samples_2[j] - average_2)
                              / (sd_1 * sd_2);
                }

                correlations[i] = result / (float)window;
            }

            // Determine best correlation
            uint32_t argmax = (std::max_element(correlations, correlations + slide * 2) - correlations);
            // Determine how much we have to slide before the best correlation is reached
            *index = argmax - slide;

            return correlations[argmax];
        }

        float decoder_impl::stddev(const float *values, uint32_t len, float mean) {
            float variance = 0.0f, temp;

            for (unsigned int i = 0; i < len; i++) {
                temp = values[i] - mean;
                variance += temp * temp;
            }

            variance /= (float)len;
            return std::sqrt(variance);
        }

        float decoder_impl::detect_upchirp(const gr_complex *samples, uint32_t window, uint32_t slide, int32_t *index) {
            float samples_ifreq[window];

            instantaneous_frequency(samples, samples_ifreq, window);
            return sliding_norm_cross_correlate(samples_ifreq, &this->d_upchirp_ifreq[0], window, slide, index);
        }

        unsigned int decoder_impl::get_shift_fft(gr_complex *samples) {
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
            for (uint32_t i = 0; i < this->d_samples_per_symbol; i++) {
                mult_hf[i] = conj(samples[i] * this->d_downchirp[i]);
            }

            samples_to_file("/tmp/mult", &mult_hf[0], this->d_samples_per_symbol, sizeof(gr_complex));

            // Perform decimation
            for (uint32_t i = 0; i < this->d_number_of_bins; i++) {
                firdecim_crcf_execute(this->d_decim, &mult_hf[this->d_decim_factor * i], &d_mult[i]);
            }

            samples_to_file("/tmp/resampled", &this->d_mult[0], this->d_number_of_bins, sizeof(gr_complex));

            // Perform FFT
            fft_execute(this->d_q);

            // Get magnitude
            for (uint32_t i = 0; i < this->d_number_of_bins; i++) {
                fft_mag[i] = abs(this->d_fft[i]);
            }

            samples_to_file("/tmp/fft", &this->d_fft[0], this->d_number_of_bins, sizeof(gr_complex));

            // Return argmax here
            return (std::max_element(fft_mag, fft_mag + this->d_number_of_bins) - fft_mag);
        }

        unsigned int decoder_impl::max_frequency_gradient_idx(gr_complex *samples) {
            float instantaneous_phase[this->d_samples_per_symbol];
            float instantaneous_freq [this->d_samples_per_symbol];
            //float bins[this->d_number_of_bins];

            samples_to_file("/tmp/data", &samples[0], this->d_samples_per_symbol, sizeof(gr_complex));

            // Determine instant phase
            for (unsigned int i = 0; i < this->d_samples_per_symbol; i++) {
                instantaneous_phase[i] = arg(samples[i]);
            }

            liquid_unwrap_phase(instantaneous_phase, this->d_samples_per_symbol);

            float max_if_diff            = 2000.0f;
            unsigned int max_if_diff_idx = 0;
            const double div             = (double)this->d_samples_per_second / (2.0f * M_PI);

            for (unsigned int i = 1; i < this->d_samples_per_symbol; i++) {
                instantaneous_freq[i - 1] = (float)((instantaneous_phase[i] - instantaneous_phase[i - 1]) * div);
            }

            uint32_t osr   = this->d_samples_per_symbol / this->d_number_of_bins;
            float last_avg = instantaneous_freq[0];

            for (unsigned int i = 0; i < this->d_number_of_bins; i++) {
                float avg = 0.0f;

                for (unsigned int j = 0; j < osr; j++) {
                    avg += instantaneous_freq[(osr * i) + j];
                }

                avg /= (float)osr;

                float diff = abs(last_avg - avg);

                if (diff > max_if_diff) {
                    max_if_diff     = diff;
                    max_if_diff_idx = i;
                }

                last_avg = avg;
            }

            //std::cout << "!!!" << max_if_diff << std::endl;

            return max_if_diff_idx;
        }

        bool decoder_impl::demodulate(gr_complex *samples, bool is_header) {
            unsigned int bin_idx = this->max_frequency_gradient_idx(samples);
            //unsigned int bin_idx = get_shift_fft(samples);
            //unsigned int bin_idx_test = get_shift_fft(samples);
            unsigned int bin_idx_test = 0;

            // Header has additional redundancy
            if (is_header) {
                bin_idx      /= 4;
                bin_idx_test /= 4;
            }

            // Decode (actually gray encode) the bin to get the symbol value
            unsigned int word = gray_encode(bin_idx);
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
            unsigned int bits_per_word = this->d_words.size();

            if (bits_per_word > 8) {
                // Not sure if this can ever occur. It would imply coding rate high than 4/8 e.g. 4/9.
                std::cerr << "More than 8 bits per word. uint8_t will not be sufficient! Bytes need to be stored in intermediate array and then packed into words_deinterleaved!" << std::endl;
            }

            std::deque<uint8_t> words_deinterleaved;
            unsigned int offset_start = ppm - 1, offset_diag, i;
            uint8_t d;

            for (i = 0; i < ppm; i++) {
                d = 0;
                offset_diag = offset_start;

                for (unsigned int j = 0; j < bits_per_word; j++) {
                    uint8_t power = 1 << j;
                    unsigned int power_check = 1 << offset_diag;

                    if (this->d_words[j] & power_check) { // Mask triggers
                        d += power;
                    }

                    if (offset_diag)    offset_diag--;
                    else                offset_diag = ppm - 1;
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
            const uint8_t *prng = NULL;
            const uint8_t shuffle_pattern[] = {7, 6, 3, 4, 2, 1, 0, 5};

            if (is_header) {
                prng = gr::lora::prng_header;
            } else {
                switch(this->d_sf) {
                    case  7: prng = gr::lora::prng_payload_sf7;  break;
                    case  8: prng = gr::lora::prng_payload_sf8;  break;
                    case  9: prng = gr::lora::prng_payload_sf9;  break;
                    case 10: prng = gr::lora::prng_payload_sf10; break;
                    case 11: prng = gr::lora::prng_payload_sf11; break;
                    case 12: prng = gr::lora::prng_payload_sf12; break;
                    default: prng = gr::lora::prng_payload_sf7;  break;
                }
            }

            this->deshuffle(shuffle_pattern, is_header);
            this->dewhiten(prng);
            this->hamming_decode(out_data);

            // Nibbles are reversed TODO why is this?
            this->nibble_reverse(out_data, this->d_payload_length);

            // Print result
            std::stringstream result;

            for (uint32_t i = 0; i < this->d_payload_length; i++) {
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

            for (uint32_t i = 0; i < to_decode; i++) {
                original = this->d_demodulated[i];
                result   = 0;

                for (uint32_t j = 0; j < len; j++) {
                    if (original & (1 << shuffle_pattern[j])) {
                        result |= 1 << j;
                    }
                }

        float sum = 0.0f;
        for(int i = 0; i < d_samples_per_symbol-1; i++) {
            sum += instantaneous_freq[i];
            #ifndef NDEBUG
                //print_vector(d_debug, d_words_deshuffled, "S", sizeof(uint8_t)*8);
                print_vector_raw(this->d_debug, this->d_words_deshuffled, sizeof(uint8_t) * 8);
                this->d_debug << std::endl;
            #endif

            // We're done with these words
            if (is_header){
                this->d_demodulated.erase(this->d_demodulated.begin(), this->d_demodulated.begin() + 5);
                this->d_demodulated.clear();
            }
        }

        void decoder_impl::dewhiten(const uint8_t *prng) {
            uint32_t i, len = this->d_words_deshuffled.size();

        /*d_cfo_estimation = (*std::max_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1) + *std::min_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1)) / 2;*/
    }

    void decoder_impl::correct_cfo(gr_complex* samples, int num_samples) {
        for(uint32_t i = 0; i < num_samples; i++) {
            samples[i] = samples[i] * gr_expj(2.0f * M_PI * -d_cfo_estimation * (d_dt * i));
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

        void decoder_impl::nibble_reverse(uint8_t *out_data, int len) {
            for (int i = 0; i < len; i++) {
                out_data[i] = ((out_data[i] & 0x0f) << 4) | ((out_data[i] & 0xf0) >> 4);
            }
        }

        void decoder_impl::determine_cfo(const gr_complex *samples) {
            float instantaneous_phase[this->d_samples_per_symbol];
//            float instantaneous_freq [this->d_samples_per_symbol];
            double div = (double) this->d_samples_per_second / (2.0f * M_PI);

            // Determine instant phase
            for (unsigned int i = 0; i < this->d_samples_per_symbol; i++) {
                instantaneous_phase[i] = arg(samples[i]);
            }

            liquid_unwrap_phase(instantaneous_phase, this->d_samples_per_symbol);

            // Determine instant freq
//            for (unsigned int i = 1; i < this->d_samples_per_symbol; i++) {
//                instantaneous_freq[i - 1] = (float)((instantaneous_phase[i] - instantaneous_phase[i - 1]) * div);
//            }

            float sum = 0.0f;

            for (uint32_t i = 1; i < this->d_samples_per_symbol; i++) {
                sum += (float)((instantaneous_phase[i] - instantaneous_phase[i - 1]) * div);
            }

            this->d_cfo_estimation = sum / (float)(this->d_samples_per_symbol - 1);

            /*d_cfo_estimation = (*std::max_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1) + *std::min_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1)) / 2;*/
        }

        void decoder_impl::correct_cfo(gr_complex *samples, uint32_t num_samples) {
            const float mul = 2.0f * M_PI * -this->d_cfo_estimation * this->d_dt;

            for (uint32_t i = 0; i < num_samples; i++) {
                samples[i] *= gr_expj(mul * i);
            }
        }

        int decoder_impl::find_preamble_start(gr_complex *samples) {
            for (uint32_t i = 0; i < this->d_samples_per_symbol; i++) {
                if (!this->get_shift_fft(&samples[i]))
                    return i;
            }

            return -1;
        }

        int decoder_impl::find_preamble_start_fast(gr_complex *samples, uint32_t len) {
            (void) len;

            const uint32_t decimation = this->d_corr_decim_factor;
            const uint32_t decim_size = this->d_samples_per_symbol / decimation;

            const float mul = (float)this->d_samples_per_second / (2.0f * M_PI);
            uint32_t rising = 0;
            static const uint32_t rising_required = 2;

    void decoder_impl::msg_lora_frame(const uint8_t *frame_bytes, uint32_t frame_len) {

    }

    int decoder_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items) {
        gr_complex* input = (gr_complex*) input_items[0];
        gr_complex* raw_input = (gr_complex*) input_items[1];
        float *out = (float*)output_items[0];

        switch(d_state) {
            case DETECT: {
                int i = find_preamble_start_fast(&input[0], 2*d_samples_per_symbol);
                if(i != -1) {
                    uint32_t c_window = std::min(2*d_samples_per_symbol - i, d_samples_per_symbol);
                    int32_t index_correction = 0;
                    float c = detect_upchirp(&input[i], c_window, d_samples_per_symbol / d_corr_decim_factor, &index_correction);
                    if(c > 0.8f) {
                        d_debug << "Cu: " << c << std::endl;
                        samples_to_file("/tmp/detectb", &input[i], d_samples_per_symbol, sizeof(gr_complex));
                        samples_to_file("/tmp/detect", &input[i+index_correction], d_samples_per_symbol, sizeof(gr_complex));
                        d_corr_fails = 0;
                        d_state = SYNC;
                        consume_each(i+index_correction);
                        break;
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
                        uint32_t c_window = std::min(2 * this->d_samples_per_symbol - i,
                                                     this->d_samples_per_symbol);
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
                            this->d_corr_fails = 0;
                            this->d_state = gr::lora::DecoderState::SYNC;
                            this->consume_each(i + index_correction);
                            break;
                        }
                    }

                    this->consume_each(2 * this->d_samples_per_symbol);
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
                        this->d_payload_length  = 3;

                        this->decode(decoded, true);

                        this->nibble_reverse(decoded, 1); // TODO: Why? Endianess?
                        this->d_payload_length  = decoded[0];
                        this->d_cr              = this->lookup_cr(decoded[1]);

                        int symbols_per_block   = this->d_cr + 4;
                        int bits_needed         = this->d_payload_length * 8 + 16;
                        float symbols_needed    = float(bits_needed) * (symbols_per_block / 4.0f) / float(this->d_sf);
                        int blocks_needed       = ceil(symbols_needed / symbols_per_block);
                        this->d_payload_symbols = blocks_needed * symbols_per_block;

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
                    std::cerr << "LoRa Decoder: No state! Shouldn't happen\n";
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

    } /* namespace lora */
} /* namespace gr */
