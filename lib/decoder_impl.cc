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
#include "decoder_impl.h"
#include "tables.h"
#include "utilities.h"

#define CORRELATION_SEARCH_RANGE 1024
#define DELAY_AFTER_SYNC 262
//#define NO_TMP_WRITES 1
//#define CFO_CORRECT 1

namespace gr {
  namespace lora {

    decoder::sptr
    decoder::make(int finetune) {
      return gnuradio::get_initial_sptr
        (new decoder_impl(finetune));
    }

    /*
     * The private constructor
     */
    decoder_impl::decoder_impl(int finetune) : gr::sync_block("decoder",
            gr::io_signature::make(1, -1, sizeof(gr_complex)),
            gr::io_signature::make(0, 2, sizeof(float))) {
        d_state = DETECT;

        d_debug_samples.open("/tmp/grlora_debug", std::ios::out | std::ios::binary);
        d_debug.open("/tmp/grlora_debug_txt", std::ios::out);
        d_sf = 7;  // Only affects PHY send
        d_bw = 125000;
        d_cr = 4;
        d_bits_per_second = (double)d_sf * 1.0f / (pow(2.0f, d_sf) / d_bw);
        d_samples_per_second = 1000000;
        d_symbols_per_second = (double)d_bw / pow(2.0f, d_sf);
        d_bits_per_symbol = (uint32_t)(d_bits_per_second / d_symbols_per_second);
        d_samples_per_symbol = (uint32_t)(d_samples_per_second / d_symbols_per_second);
        d_number_of_bins = (uint32_t)pow(2, d_sf);
        d_number_of_bins_hdr = d_number_of_bins / 4;
        d_compression = 8;
        d_payload_symbols = 0;
        d_finetune = finetune;
        d_cfo_estimation = 0.0f;
        d_cfo_step = 0;
        d_dt = 1.0f / d_samples_per_second;

        // Some preparations
        std::cout << "Bits per symbol: " << d_bits_per_symbol << std::endl;
        std::cout << "Bins per symbol: " << d_number_of_bins << std::endl;
        std::cout << "Header bins per symbol: " << d_number_of_bins_hdr << std::endl;
        std::cout << "Samples per symbol: " << d_samples_per_symbol << std::endl;
        std::cout << "Decimation: " << d_samples_per_symbol / d_number_of_bins << std::endl;

        build_ideal_downchirp();
        set_output_multiple(2*d_samples_per_symbol);
        d_fft.resize(d_number_of_bins);
        d_mult.resize(d_number_of_bins);
        d_q = fft_create_plan(d_number_of_bins, &d_mult[0], &d_fft[0], LIQUID_FFT_FORWARD, 0);

        // Decimation filter
        float g[DECIMATOR_FILTER_SIZE];
        liquid_firdes_rrcos(8, 1, 0.5f, 0.3f, g); // Filter for interpolating
        for (uint32_t i = 0; i < DECIMATOR_FILTER_SIZE; i++) // Reverse it to get decimation filter
            d_decim_h[i] = g[DECIMATOR_FILTER_SIZE-i-1];
        d_decim_factor = d_samples_per_symbol / d_number_of_bins;

        d_decim = firdecim_crcf_create(d_decim_factor, d_decim_h, DECIMATOR_FILTER_SIZE);
    }

    /*
     * Our virtual destructor.
     */
    decoder_impl::~decoder_impl() {
        if(d_debug_samples.is_open())
            d_debug_samples.close();
        if(d_debug.is_open())
            d_debug.close();

        fft_destroy_plan(d_q);
        firdecim_crcf_destroy(d_decim);
    }

    void decoder_impl::build_ideal_downchirp(void) {
        d_downchirp.resize(d_samples_per_symbol);
        d_downchirp_fft.resize(d_samples_per_symbol);

        double T = 1.0f / d_symbols_per_second;
        double dir = -1.0f;
        double f0 = (d_bw / 2.0f);
        double amplitude = 1.0f;

        // Store time domain signal
        for(int i = 0; i < d_samples_per_symbol; i++) { // Width in number of samples = samples_per_symbol
            // See https://en.wikipedia.org/wiki/Chirp#Linear
            double t = d_dt * i;
            d_downchirp[i] = gr_complex(amplitude, amplitude) * gr_expj(2.0f * M_PI * (f0 * t + (dir * (0.5 * d_bw / T) * pow(t, 2))));
        }

        // Store FFT of downchirp TODO needed?
        int flags = 0;
        fftplan q = fft_create_plan(d_samples_per_symbol, &d_downchirp[0], &d_downchirp_fft[0], LIQUID_FFT_FORWARD, flags);
        fft_execute(q);
        fft_destroy_plan(q);

        samples_to_file("/tmp/downchirp", &d_downchirp[0], d_downchirp.size(), sizeof(gr_complex));
        samples_to_file("/tmp/downchirp_fft", &d_downchirp_fft[0], d_downchirp_fft.size(), sizeof(gr_complex));
    }

    void decoder_impl::samples_to_file(const std::string path, const gr_complex* v, int length, int elem_size) {
        #ifndef NO_TMP_WRITES
            std::ofstream out_file;
            out_file.open(path.c_str(), std::ios::out | std::ios::binary);
            //for(std::vector<gr_complex>::const_iterator it = v.begin(); it != v.end(); ++it) {
            for(uint32_t i = 0; i < length; i++) {
                out_file.write(reinterpret_cast<const char *>(&v[i]), elem_size);
            }
            out_file.close();
        #endif
    }

    void decoder_impl::samples_debug(const gr_complex* v, int length) {
        gr_complex start_indicator(0.0f,32.0f);
        d_debug_samples.write(reinterpret_cast<const char *>(&start_indicator), sizeof(gr_complex));
        for(uint32_t i = 1; i < length; i++) {
            d_debug_samples.write(reinterpret_cast<const char *>(&v[i]), sizeof(gr_complex));
        }
    }

    bool decoder_impl::calc_energy_threshold(gr_complex* samples, int window_size, float threshold) {
        float result = 0.0f;
        for(int i = 0; i < window_size; i++) {
            result += std::pow(abs(samples[i]), 2);
        }
        result /= (float)window_size;

        //d_debug << "T: " << result << "\n";

        if(result > threshold) {
            return true;
        } else {
            return false;
        }
    }

    inline void decoder_impl::phase(gr_complex* in_samples, float* out_phase, int window) {
        for(int i = 0; i < window; i++) {
            out_phase[i] = arg(in_samples[i]); // = the same as atan2(imag(in_samples[i]),real(in_samples[i]));
        }
    }

    double decoder_impl::cross_correlate(const gr_complex *samples_1, const gr_complex *samples_2, int window) {
        double result = 0.0f;

        for (int i = 0; i < window; i++) {
            result += real(samples_1[i] * conj(samples_2[i]));
        }

        result = result / window;
        return result;
    }

    double decoder_impl::freq_cross_correlate(const gr_complex *samples_1, const gr_complex *samples_2, int window) {
        double result = 0.0f;
        float instantaneous_phase[window];
        float instantaneous_phase_down[window];
        float instantaneous_freq[window];
        float instantaneous_freq_down[window];

        // Determine instant phase
        for(unsigned int i = 0; i < window; i++) {
            instantaneous_phase[i] = arg(samples_1[i]);
            instantaneous_phase_down[i] = arg(samples_2[i]);
        }
        liquid_unwrap_phase(instantaneous_phase, window);
        liquid_unwrap_phase(instantaneous_phase_down, window);

        // Instant freq
        for(unsigned int i = 1; i < window; i++) {
            instantaneous_freq[i-1] = instantaneous_phase[i] - instantaneous_phase[i-1];
            instantaneous_freq_down[i-1] = instantaneous_phase_down[i] - instantaneous_phase_down[i-1];
        }

        for (int i = 0; i < window-1; i++) {
            result += instantaneous_freq[i] * instantaneous_freq_down[i];
        }

        result = result / window;
        return result;
    }

    unsigned int decoder_impl::sync_fft(gr_complex* samples) {
        float fft_mag[d_number_of_bins];
        gr_complex mult_hf[d_samples_per_symbol];

        /*#ifdef CFO_CORRECT
            determine_cfo(&samples[0]);
            std::cout << "CFO: " << d_cfo_estimation << std::endl;
            correct_cfo(&samples[0], d_samples_per_symbol);
        #endif*/

        samples_to_file("/tmp/data", &samples[0], d_samples_per_symbol, sizeof(gr_complex));

        // Multiply with ideal downchirp
        for(uint32_t i = 0; i < d_samples_per_symbol; i++) {
            mult_hf[i] = conj(samples[i] * d_downchirp[i]);
        }

        samples_to_file("/tmp/mult", &mult_hf[0], d_samples_per_symbol, sizeof(gr_complex));

        // Perform decimation
        for (uint32_t i = 0; i < d_number_of_bins; i++) {
            firdecim_crcf_execute(d_decim, &mult_hf[d_decim_factor*i], &d_mult[i]);
        }

        samples_to_file("/tmp/resampled", &d_mult[0], d_number_of_bins, sizeof(gr_complex));

        // Perform FFT
        fft_execute(d_q);

        // Get magnitude
        for(int i = 0; i < d_number_of_bins; i++) {
            fft_mag[i] = abs(d_fft[i]);
        }

        samples_to_file("/tmp/fft", &d_fft[0], d_number_of_bins, sizeof(gr_complex));

        // Return argmax here
        return (std::max_element(fft_mag,fft_mag+d_number_of_bins) - fft_mag);
    }

    unsigned int decoder_impl::max_frequency_gradient_idx(gr_complex* samples) {
        float instantaneous_phase[d_samples_per_symbol];
        float instantaneous_freq[d_samples_per_symbol];
        float bins[d_number_of_bins];

        // Determine instant phase
        for(unsigned int i = 0; i < d_samples_per_symbol; i++) {
            instantaneous_phase[i] = arg(samples[i]);
        }
        liquid_unwrap_phase(instantaneous_phase, d_samples_per_symbol);

        float max_if_diff = 2000.0f;
        unsigned int max_if_diff_idx = 0;

        for(unsigned int i = 1; i < d_samples_per_symbol; i++) {
            float ifreq = (instantaneous_phase[i] - instantaneous_phase[i-1]) / (2.0f * M_PI) * d_samples_per_second; // TODO: constant multiplication can be removed
            instantaneous_freq[i-1] = ifreq;
        }

        int osr = d_samples_per_symbol / d_number_of_bins;
        float last_avg = instantaneous_freq[0];
        for(unsigned int i = 0; i < d_number_of_bins; i++) {
            float avg = 0.0f;
            for(unsigned int j = 0; j < osr; j++) {
                avg += instantaneous_freq[(osr*i) + j];
            }
            avg /= osr;

            float diff = abs(last_avg - avg);

            if(diff > max_if_diff) {
                max_if_diff = diff;
                max_if_diff_idx = i;
            }

            last_avg = avg;
        }
        //std::cout << "!!!" << max_if_diff << std::endl;

        return max_if_diff_idx;
    }

    bool decoder_impl::demodulate(gr_complex* samples, bool is_header) {
        unsigned int bin_idx = max_frequency_gradient_idx(samples);
        //unsigned int bin_idx = sync_fft(samples);
        //unsigned int bin_idx_test = sync_fft(samples);
        unsigned int bin_idx_test = 0;

        // Header has additional redundancy
        if(is_header) {
            bin_idx /= 4;
            bin_idx_test /= 4;
        }

        // Decode (actually gray encode) the bin to get the symbol value
        unsigned int word = gray_encode(bin_idx);
        d_debug << bin_idx << " " << to_bin(word, is_header ? 5 : 7) << " ! " << bin_idx_test << std::endl;
        d_words.push_back(word);

        // Look for 4+cr symbols and stop
        if(d_words.size() == (4 + d_cr)) {
            // Deinterleave
            if(is_header) {
                //print_vector(d_words, "M", d_sf - 2);
                deinterleave(d_sf - 2);
            } else {
                //print_vector(d_words, "M", d_sf);
                deinterleave(d_sf);
            }

            return true; // Signal that a block is ready for decoding
        }

        return false; // We need more words in order to decode a block
    }

    void decoder_impl::deinterleave(int ppm) {
        unsigned int bits_per_word = d_words.size();

        if(bits_per_word > 8) { // Not sure if this can ever occur. It would imply coding rate high than 4/8 e.g. 4/9.
            std::cout << "More than 8 bits per word. uint8_t will not be sufficient! Bytes need to be stored in intermediate array and then packed into words_deinterleaved!" << std::endl;
        }

        unsigned int offset_start = ppm-1;
        std::vector<uint8_t> words_deinterleaved;
        for(unsigned int i = 0; i < ppm; i++) {
            uint8_t d = 0;
            unsigned int offset_diag = offset_start;

            for(unsigned int j = 0; j < bits_per_word; j++) {
                uint8_t power = pow(2, j);
                unsigned int power_check = pow(2, offset_diag); // TODO: Here we are actually reversing endianess. This needs to be fixed in the future by implementing the interleaving similarly to how it is done in the Python based decoder.
                if((d_words[j] & power_check) > 0) { // Mask triggers
                    d += power;
                }

                if(offset_diag == 0)
                    offset_diag = ppm-1;
                else
                    offset_diag -= 1;
            }

            offset_start -= 1;
            words_deinterleaved.push_back(d);
        }

        //print_vector(words_deinterleaved, "D", sizeof(uint8_t)*8);
        std::reverse(words_deinterleaved.begin(),words_deinterleaved.end());

        // Add to demodulated data
        for(int i = 0; i < words_deinterleaved.size(); i++) {
            d_demodulated.push_back(words_deinterleaved[i]);
        }

        // Cleanup
        d_words.clear();
    }

    int decoder_impl::decode(uint8_t* out_data, bool is_header) {
        const uint8_t* prng = NULL;
        const uint8_t shuffle_pattern[] = {7, 6, 3, 4, 2, 1, 0, 5};

        if(is_header) {
            prng = prng_header;
        } else {
            prng = prng_payload;
        }

        deshuffle(shuffle_pattern);
        dewhiten(prng);
        hamming_decode(out_data);

        // Nibbles are reversed
        nibble_reverse(out_data, d_payload_length);

        // Print result
        std::stringstream result;
        for (int i = 0; i < d_payload_length; i++) {
            result << " " << std::hex << std::setw(2) << std::setfill('0') << (int)out_data[i];
        }

        if(!is_header)
            std::cout << result.str() << std::endl;

        return 0;
    }

    void decoder_impl::deshuffle(const uint8_t* shuffle_pattern) {
        for(int i = 0; i < d_demodulated.size(); i++) {
            uint8_t original = d_demodulated[i];
            uint8_t result = 0;

            for(int j = 0; j < sizeof(shuffle_pattern) / sizeof(uint8_t); j++) {
                uint8_t mask = pow(2, shuffle_pattern[j]);
                if((original & mask) > 0) {
                    result += pow(2, j);
                }
            }

            d_words_deshuffled.push_back(result);
        }

        //print_vector(d_words_deshuffled, "S", sizeof(uint8_t)*8);

        // We're done with these words
        d_demodulated.clear();
    }

    void decoder_impl::dewhiten(const uint8_t* prng) {
        for(int i = 0; i < d_words_deshuffled.size(); i++) {
            uint8_t xor_b = d_words_deshuffled[i] ^ prng[i];
            xor_b = (xor_b & 0xF0) >> 4 | (xor_b & 0x0F) << 4; // TODO: reverse bit order is performed here, but is probably due to mistake in interleaving
            xor_b = (xor_b & 0xCC) >> 2 | (xor_b & 0x33) << 2;
            xor_b = (xor_b & 0xAA) >> 1 | (xor_b & 0x55) << 1;
            d_words_dewhitened.push_back(xor_b);
        }

        //print_vector(d_words_dewhitened, "W", sizeof(uint8_t)*8);

        d_words_deshuffled.clear();
    }

    void decoder_impl::hamming_decode(uint8_t* out_data) {
        unsigned int n = ceil(d_words_dewhitened.size() * 4.0 / 8.0);
        fec_scheme fs = LIQUID_FEC_HAMMING84;

        unsigned int k = fec_get_enc_msg_length(fs, n);
        fec hamming = fec_create(fs, NULL);

        fec_decode(hamming, n, &d_words_dewhitened[0], out_data);

        d_words_dewhitened.clear();
        fec_destroy(hamming);
    }

    void decoder_impl::nibble_reverse(uint8_t* out_data, int len) {
        for(int i = 0; i < len; i++) {
            out_data[i] = ((out_data[i] & 0x0f) << 4) | ((out_data[i] & 0xf0) >> 4);
        }
    }

    void decoder_impl::determine_cfo(const gr_complex* samples) {
        float instantaneous_phase[d_samples_per_symbol];
        float instantaneous_freq[d_samples_per_symbol];

        // Determine instant phase
        for(unsigned int i = 0; i < d_samples_per_symbol; i++) {
            instantaneous_phase[i] = arg(samples[i]);
        }
        liquid_unwrap_phase(instantaneous_phase, d_samples_per_symbol);

        // Determine instant freq
        for(unsigned int i = 1; i < d_samples_per_symbol; i++) {
            float ifreq = (instantaneous_phase[i] - instantaneous_phase[i-1]) / (2.0f * M_PI) * d_samples_per_second;
            instantaneous_freq[i-1] = ifreq;
        }

        float sum = 0.0f;
        for(int i = 0; i < d_samples_per_symbol; i++) {
            sum += instantaneous_freq[i];
        }
        sum /= d_samples_per_symbol;

        d_cfo_estimation = sum;

        /*d_cfo_estimation = (*std::max_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1) + *std::min_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1)) / 2;*/
    }

    void decoder_impl::correct_cfo(gr_complex* samples, int num_samples) {
        for(uint32_t i = 0; i < num_samples; i++) {
            samples[i] = samples[i] * gr_expj(2.0f * M_PI * -d_cfo_estimation * (d_dt * d_cfo_step));
            d_cfo_step += 1;
        }
    }

    int decoder_impl::find_preamble_start(gr_complex* samples) {
        for(int i = 0; i < d_samples_per_symbol; i++) {
            unsigned int c = sync_fft(&samples[i]);
            if(c == 0) {
                return i;
            }
        }
    }

    int decoder_impl::find_preamble_start_fast(gr_complex* samples, uint32_t len) {
        int step_size = d_samples_per_symbol / 8;
        for(int i = 0; i < len; i += 8) {
            bool higher = true;
            float last_ifreq = -999999999;

            for(int j = 0; j < 8; j++) {
                float s[2] = {
                    arg(samples[i+(j*step_size)]),
                    arg(samples[i+(j*step_size)+1])
                };
                liquid_unwrap_phase(s, 2);

                float ifreq = (s[1] - s[0]) / (2.0f * M_PI) * d_samples_per_second;
                d_debug << "F: " << ifreq << std::endl;

                if(ifreq - last_ifreq < (d_bw / 8) / 1.5) { // Make sure it rises fast enough
                    higher = false;
                    d_debug << "NOPE" << std::endl;
                    break;
                } else {
                    last_ifreq = ifreq;
                }
            }

            if(higher) {
                d_debug << "YAY" << std::endl;
                return i;
            }
        }

        return -1;
    }

    int decoder_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items) {
        gr_complex * input = (gr_complex*) input_items[0];
        float *out = (float*)output_items[0];

        switch(d_state) {
            case DETECT: {
                if(calc_energy_threshold(input, noutput_items, 0.002)) {
                    // Attempt to synchronize to an upchirp of the preamble
                    int chirp_start_pos = -1;
                    d_cfo_estimation = 0;

                    // Find rough position of preamble
                    int i = find_preamble_start_fast(&input[0], noutput_items);

                    // After this step, if i != -1 we know that we are in a rising chirp, starting from i.
                    // Calculate the CFO here, and correct for it. Then perform sync_fft until we get a 0
                    // The final position where this is the case indicates the start of the preamble.
                    if(i != -1) {
                        // TODO: Find algorithm to reliably determine CFO
                        samples_to_file("/tmp/bcfo", &input[0], noutput_items, sizeof(gr_complex));
                        i = find_preamble_start(&input[0]);
                        determine_cfo(&input[i]);
                        d_debug << "CFO " << d_cfo_estimation << std::endl;
                        correct_cfo(&input[0], noutput_items);
                        samples_to_file("/tmp/acfo", &input[0], noutput_items, sizeof(gr_complex));

                        // Sync
                        i = find_preamble_start(&input[0]);
                        chirp_start_pos = i;
                        d_debug << "DETECT: Preamble starts at " << i << std::endl;
                        samples_to_file("/tmp/detect", &input[chirp_start_pos + d_finetune], d_samples_per_symbol, sizeof(gr_complex));


                        d_state = SYNC;
                        d_corr_fails = 0;
                        consume_each(chirp_start_pos + d_finetune);
                    } else {
                        consume_each(noutput_items);
                    }
                } else {
                    consume_each(noutput_items);
                }
                break;
            }
            case SYNC: {
                double c = freq_cross_correlate(&input[0], &d_downchirp[0], d_samples_per_symbol);
                d_debug << "C: " << c << std::endl;

                if(c > 0.045f) {
                    d_debug << "SYNC: " << c << std::endl;
                    // Debug stuff
                    samples_to_file("/tmp/sync", &input[0], CORRELATION_SEARCH_RANGE, sizeof(gr_complex));

                    d_state = PAUSE;
                    consume_each(d_samples_per_symbol);
                } else {
                    d_corr_fails++;
                    if(d_corr_fails > 32) {
                        d_state = DETECT;
                    }
                    consume_each(d_samples_per_symbol);
                }
                break;
            }
            case PAUSE: {
                d_state = DECODE_HEADER;

                samples_debug(input, d_samples_per_symbol + DELAY_AFTER_SYNC);
                consume_each(d_samples_per_symbol + DELAY_AFTER_SYNC);
                break;
            }
            case DECODE_HEADER: {
                if(demodulate(input, true)) {
                    uint8_t decoded[3];
                    d_payload_length = 3; // TODO: A bit messy. I think it's better to make an internal decoded std::vector
                    decode(decoded, true);

                    nibble_reverse(decoded, 1); // TODO: Why?
                    d_payload_length = decoded[0];
                    d_cr = 4; // TODO: Get from header instead of hardcode

                    int symbols_per_block = d_cr + 4;
                    int bits_needed = ((d_payload_length * 8) + 16) * (symbols_per_block / 4);
                    float symbols_needed = float(bits_needed) / float(d_sf);
                    int blocks_needed = ceil(symbols_needed / symbols_per_block);
                    d_payload_symbols = blocks_needed * symbols_per_block;

                    d_debug << "LEN: " << d_payload_length << " (" << d_payload_symbols << " symbols)" << std::endl;

                    d_state = DECODE_PAYLOAD;
                }

                samples_debug(input, d_samples_per_symbol);
                consume_each(d_samples_per_symbol);
                break;
            }
            case DECODE_PAYLOAD: {
                if(demodulate(input, false)) {
                    d_payload_symbols -= (4 + d_cr);

                    if(d_payload_symbols <= 0) {
                        uint8_t decoded[d_payload_length];
                        decode(decoded, false);

                        d_state = DETECT;
                    }
                }

                samples_debug(input, d_samples_per_symbol);
                consume_each(d_samples_per_symbol);
                break;
            }
            case STOP: {
                consume_each(d_samples_per_symbol);
                break;
            }
            default: {
                std::cout << "Shouldn't happen\n";
                break;
            }
        }

        // Tell runtime system how many output items we produced.
        return 0;
    }

    void decoder_impl::set_finetune(int32_t finetune) {
        d_finetune = finetune;
    }

  } /* namespace lora */
} /* namespace gr */
