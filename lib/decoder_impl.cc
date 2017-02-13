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

//#define NO_TMP_WRITES 1
//#define CFO_CORRECT 1

namespace gr {
  namespace lora {

    decoder::sptr
    decoder::make(float samp_rate, int sf) {
      return gnuradio::get_initial_sptr
        (new decoder_impl(samp_rate, sf));
    }

    /*
     * The private constructor
     */
    decoder_impl::decoder_impl(float samp_rate, int sf) : gr::sync_block("decoder",
            gr::io_signature::make(1, -1, sizeof(gr_complex)),
            gr::io_signature::make(0, 2, sizeof(float))) {
        d_state = DETECT;

        d_debug_samples.open("/tmp/grlora_debug", std::ios::out | std::ios::binary);
        d_debug.open("/tmp/grlora_debug_txt", std::ios::out);
        d_sf = sf;  // Only affects PHY send
        d_bw = 125000;
        d_cr = 4;
        d_bits_per_second = (double)d_sf * (4.0f/4.0f+d_cr) / (pow(2.0f, d_sf) / d_bw);
        d_samples_per_second = samp_rate;
        d_symbols_per_second = (double)d_bw / pow(2.0f, d_sf);
        d_bits_per_symbol = (uint32_t)(d_bits_per_second / d_symbols_per_second);
        d_samples_per_symbol = (uint32_t)(d_samples_per_second / d_symbols_per_second);
        d_delay_after_sync = d_samples_per_symbol / 4;
        d_corr_decim_factor = 8; // samples_per_symbol / corr_decim_factor = correlation window. Also serves as preamble decimation factor
        d_number_of_bins = (uint32_t)pow(2, d_sf);
        d_number_of_bins_hdr = d_number_of_bins / 4;
        d_payload_symbols = 0;
        d_cfo_estimation = 0.0f;
        d_dt = 1.0f / d_samples_per_second;

        // Some preparations
        std::cout << "Bits per symbol: " << d_bits_per_symbol << std::endl;
        std::cout << "Bins per symbol: " << d_number_of_bins << std::endl;
        std::cout << "Header bins per symbol: " << d_number_of_bins_hdr << std::endl;
        std::cout << "Samples per symbol: " << d_samples_per_symbol << std::endl;
        std::cout << "Decimation: " << d_samples_per_symbol / d_number_of_bins << std::endl;

        build_ideal_chirps();

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

        // Register gnuradio ports
        message_port_register_out(pmt::mp("frames"));
        message_port_register_out(pmt::mp("debug"));
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

    void decoder_impl::build_ideal_chirps(void) {
        d_downchirp.resize(d_samples_per_symbol);
        d_upchirp.resize(d_samples_per_symbol);
        d_downchirp_ifreq.resize(d_samples_per_symbol);
        d_upchirp_ifreq.resize(d_samples_per_symbol);

        double dir;
        double T = 1.0f / d_symbols_per_second;
        double f0 = (d_bw / 2.0f);
        double amplitude = 1.0f;

        // Store time domain signal
        dir = 1.0f;
        for(int i = 0; i < d_samples_per_symbol; i++) { // Width in number of samples = samples_per_symbol
            // See https://en.wikipedia.org/wiki/Chirp#Linear
            double t = d_dt * i;
            d_downchirp[i] = gr_complex(amplitude, amplitude) * gr_expj(dir * 2.0f * M_PI * (f0 * t + (-1.0f * (0.5 * d_bw / T) * pow(t, 2))));
        }

        dir = -1.0f;
        for(int i = 0; i < d_samples_per_symbol; i++) {
            double t = d_dt * i;
            d_upchirp[i] = gr_complex(amplitude, amplitude) * gr_expj(dir * 2.0f * M_PI * (f0 * t + (-1.0f * (0.5 * d_bw / T) * pow(t, 2))));
        }

        // Store instant. frequency
        instantaneous_frequency(&d_downchirp[0], &d_downchirp_ifreq[0], d_samples_per_symbol);
        instantaneous_frequency(&d_upchirp[0], &d_upchirp_ifreq[0], d_samples_per_symbol);

        samples_to_file("/tmp/downchirp", &d_downchirp[0], d_downchirp.size(), sizeof(gr_complex));
        samples_to_file("/tmp/upchirp", &d_upchirp[0], d_upchirp.size(), sizeof(gr_complex));
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

    void decoder_impl::instantaneous_frequency(const gr_complex* in_samples, float* out_ifreq, uint32_t window) {
        float iphase[window];

        if(window < 2) {
            // TODO: throw warning here
            return;
        }

        instantaneous_phase(in_samples, iphase, window);

        // Instant freq
        for(uint32_t i = 1; i < window; i++) {
            out_ifreq[i-1] = iphase[i] - iphase[i-1];
        }
        out_ifreq[window-1] = out_ifreq[window-2]; // Make sure there is no strong gradient if this value is accessed by mistake
    }

    inline void decoder_impl::instantaneous_phase(const gr_complex* in_samples, float* out_iphase, uint32_t window) {
        for(uint32_t i = 0; i < window; i++) {
            out_iphase[i] = arg(in_samples[i]); // = the same as atan2(imag(in_samples[i]),real(in_samples[i]));
        }

        liquid_unwrap_phase(out_iphase, window);
    }

    float decoder_impl::cross_correlate(const gr_complex *samples_1, const gr_complex *samples_2, int window) {
        float result = 0.0f;

        for (int i = 0; i < window; i++) {
            result += real(samples_1[i] * conj(samples_2[i]));
        }

        result = result / window;
        return result;
    }

    float decoder_impl::detect_downchirp(const gr_complex *samples, uint32_t window) {
        float samples_ifreq[window];

        instantaneous_frequency(samples, samples_ifreq, window);
        return norm_cross_correlate(samples_ifreq, &d_downchirp_ifreq[0], window);
    }

    /**
     * Calculate normalized cross correlation of real values.
     * See https://en.wikipedia.org/wiki/Cross-correlation#Normalized_cross-correlation.
     */
    float decoder_impl::norm_cross_correlate(const float *samples_1, const float *samples_2, uint32_t window) {
        float result = 0.0f;

        double average_1 = std::accumulate(samples_1, samples_1 + window, 0.0) / window;
        double average_2 = std::accumulate(samples_2, samples_2 + window, 0.0) / window;
        double sd_1 = stddev(samples_1, window, average_1);
        double sd_2 = stddev(samples_2, window, average_2);

        for (int i = 0; i < window-1; i++) {
            result += (samples_1[i] - average_1) * (samples_2[i] - average_2) / (sd_1 * sd_2);
        }
        result = result / (window-1);

        return result;
    }

    float decoder_impl::sliding_norm_cross_correlate(const float *samples_1, const float *samples_2, uint32_t window, uint32_t slide, int32_t* index) {
        float correlations[slide*2];
        float samples_1_padded[window+slide*2];

        double average_1 = std::accumulate(samples_1, samples_1 + window, 0.0) / window;
        double average_2 = std::accumulate(samples_2, samples_2 + window, 0.0) / window;
        double sd_1 = stddev(samples_1, window, average_1);
        double sd_2 = stddev(samples_2, window, average_2);

        // Create padding on both sides of the samples
        for(uint32_t i = 0; i < window+slide*2; i++) {
            samples_1_padded[i] = 0.0f;
        }
        for(uint32_t i = 0; i < window; i++) {
            samples_1_padded[i+slide-1] = samples_1[i];
        }

        // Slide and correlate
        for(uint32_t i = 0; i < 2*slide; i++) {
            float result = 0.0f;
            for (uint32_t j = 0; j < window; j++) {
                result += (samples_1_padded[i+j] - average_1) * (samples_2[j] - average_2) / (sd_1 * sd_2);
            }
            correlations[i] = result / window;
        }

        uint32_t argmax = (std::max_element(correlations,correlations+slide*2) - correlations); // Determine best correlation
        *index = argmax - slide; // Determine how much we have to slide before the best correlation is reached

        return correlations[argmax];
    }

    float decoder_impl::stddev(const float *values, int len, float mean) {
        double variance = 0.0f;

        for (unsigned int i = 0; i < len; i++) {
          variance += std::pow(values[i] - mean, 2);
        }

        variance /= len;
        return std::sqrt(variance);
    }

    float decoder_impl::detect_upchirp(const gr_complex *samples, uint32_t window, uint32_t slide, int32_t* index) {
        float samples_ifreq[window];

        instantaneous_frequency(samples, samples_ifreq, window);
        return sliding_norm_cross_correlate(samples_ifreq, &d_upchirp_ifreq[0], window, slide, index);
    }

    unsigned int decoder_impl::get_shift_fft(gr_complex* samples) {
        float fft_mag[d_number_of_bins];
        gr_complex mult_hf[d_samples_per_symbol];

        #ifdef CFO_CORRECT
            determine_cfo(&samples[0]);
            d_debug << "CFO: " << d_cfo_estimation << std::endl;
            correct_cfo(&samples[0], d_samples_per_symbol);
        #endif

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

        samples_to_file("/tmp/data", &samples[0], d_samples_per_symbol, sizeof(gr_complex));

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
        //unsigned int bin_idx = get_shift_fft(samples);
        //unsigned int bin_idx_test = get_shift_fft(samples);
        unsigned int bin_idx_test = 0;

        // Header has additional redundancy
        if(is_header) {
            bin_idx /= 4;
            bin_idx_test /= 4;
        }

        // Decode (actually gray encode) the bin to get the symbol value
        unsigned int word = gray_encode(bin_idx);
        d_debug << to_bin(word, is_header ? d_sf-2 : d_sf) << " " << bin_idx  << std::endl;
        d_words.push_back(word);

        // Look for 4+cr symbols and stop
        if(d_words.size() == (4 + d_cr)) {
            // Deinterleave
            if(is_header) {
                deinterleave(d_sf - 2);
            } else {
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
                unsigned int power_check = pow(2, offset_diag);
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

        std::reverse(words_deinterleaved.begin(),words_deinterleaved.end());
        print_vector(d_debug, words_deinterleaved, "D", sizeof(uint8_t)*8);

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
            if(d_sf == 7)
                prng = prng_payload_sf7;
            else if(d_sf == 8)
                prng = prng_payload_sf8;
            else if(d_sf == 9)
                prng = prng_payload_sf9;
            else if(d_sf == 10)
                prng = prng_payload_sf10;
            else if(d_sf == 11)
                prng = prng_payload_sf11;
            else if(d_sf == 12)
                prng = prng_payload_sf12;
            else
                prng = prng_payload_sf7;
        }

        deshuffle(shuffle_pattern, is_header);
        dewhiten(prng);
        hamming_decode(out_data);

        // Nibbles are reversed TODO why is this?
        nibble_reverse(out_data, d_payload_length);

        // Print result
        std::stringstream result;
        for (int i = 0; i < d_payload_length; i++) {
            result << " " << std::hex << std::setw(2) << std::setfill('0') << (int)out_data[i];
        }

        if(!is_header) {
            d_data.insert(d_data.end(), out_data, out_data + d_payload_length);
            std::cout << result.str() << std::endl;

            pmt::pmt_t payload_blob = pmt::make_blob(&d_data[0], sizeof(uint8_t) * (d_payload_length + 3));
            message_port_pub(pmt::mp("frames"), payload_blob);
        } else {
            d_data.insert(d_data.end(), out_data, out_data + 3);
            std::cout << result.str();
        }

        return 0;
    }

    void decoder_impl::deshuffle(const uint8_t* shuffle_pattern, bool is_header) {
        uint32_t to_decode = d_demodulated.size();

        if(is_header)
            to_decode = 5;

        for(uint32_t i = 0; i < to_decode; i++) {
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

        //print_vector(d_debug, d_words_deshuffled, "S", sizeof(uint8_t)*8);
        print_vector_raw(d_debug, d_words_deshuffled, sizeof(uint8_t)*8);
        d_debug << std::endl;

        // We're done with these words
        if(is_header)
            d_demodulated.erase(d_demodulated.begin(), d_demodulated.begin()+5);
        else
            d_demodulated.clear();
    }

    void decoder_impl::dewhiten(const uint8_t* prng) {
        for(int i = 0; i < d_words_deshuffled.size(); i++) {
            uint8_t xor_b = d_words_deshuffled[i] ^ prng[i];
            xor_b = (xor_b & 0xF0) >> 4 | (xor_b & 0x0F) << 4; // TODO: reverse bit order is performed here, but is probably due to mistake in whitening or interleaving
            xor_b = (xor_b & 0xCC) >> 2 | (xor_b & 0x33) << 2;
            xor_b = (xor_b & 0xAA) >> 1 | (xor_b & 0x55) << 1;
            d_words_dewhitened.push_back(xor_b);
        }

        print_vector(d_debug, d_words_dewhitened, "W", sizeof(uint8_t)*8);

        d_words_deshuffled.clear();
    }

    void decoder_impl::hamming_decode(uint8_t* out_data) {
        uint8_t data_indices[4] = {1, 2, 3, 5};
        unsigned int n = ceil(d_words_dewhitened.size() * 4.0f / (4.0f + d_cr));
        fec_scheme fs = LIQUID_FEC_HAMMING84;

        if(d_cr == 4) {
            hamming_decode_soft(&d_words_dewhitened[0], d_words_dewhitened.size(), out_data);
            d_words_dewhitened.clear();
            return;
        } else if(d_cr == 3) {
            hamming_decode_soft(&d_words_dewhitened[0], d_words_dewhitened.size(), out_data);
            d_words_dewhitened.clear();
            return;
        } else if(d_cr == 2) {
            fec_extract_data_only(&d_words_dewhitened[0], d_words_dewhitened.size(), data_indices, 4, out_data);
            d_words_dewhitened.clear();
            return;
        } else if(d_cr == 1) { // TODO: Report parity error to the user
            fec_extract_data_only(&d_words_dewhitened[0], d_words_dewhitened.size(), data_indices, 4, out_data);
            d_words_dewhitened.clear();
            return;
        }

        /*fs = LIQUID_FEC_HAMMING84;

        unsigned int k = fec_get_enc_msg_length(fs, n);
        fec hamming = fec_create(fs, NULL);

        fec_decode(hamming, n, &d_words_dewhitened[0], out_data);

        d_words_dewhitened.clear();
        fec_destroy(hamming);*/
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
        for(int i = 0; i < d_samples_per_symbol-1; i++) {
            sum += instantaneous_freq[i];
        }
        sum /= d_samples_per_symbol-1;

        d_cfo_estimation = sum;

        /*d_cfo_estimation = (*std::max_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1) + *std::min_element(instantaneous_freq, instantaneous_freq+d_samples_per_symbol-1)) / 2;*/
    }

    void decoder_impl::correct_cfo(gr_complex* samples, int num_samples) {
        for(uint32_t i = 0; i < num_samples; i++) {
            samples[i] = samples[i] * gr_expj(2.0f * M_PI * -d_cfo_estimation * (d_dt * i));
        }
    }

    int decoder_impl::find_preamble_start(gr_complex* samples) {
        for(int i = 0; i < d_samples_per_symbol; i++) {
            unsigned int c = get_shift_fft(&samples[i]);
            if(c == 0) {
                return i;
            }
        }
    }

    int decoder_impl::find_preamble_start_fast(gr_complex* samples, uint32_t len) {
        int decimation = d_corr_decim_factor;
        int decim_size = d_samples_per_symbol / decimation;
        float decim[decimation];
        float gradient[decimation];
        uint32_t rising = 0;
        uint32_t rising_required = 2;

        gradient[0] = 0.0f;


        for(int i = 0; i < decimation; i++) {
            float s[2] = {
                arg(samples[i*decim_size]),
                arg(samples[(i+1)*decim_size])
            };
            liquid_unwrap_phase(s, 2);

            decim[i] = (s[1] - s[0]) / (2.0f * M_PI) * d_samples_per_second;
        }

        for(int i = 1; i < decimation; i++) {
            gradient[i] = decim[i] - decim[i-1];
            if(gradient[i] > gradient[i-1])
                rising++;
            if(rising >= rising_required && gradient[i] <= -20000) { // TODO: Make this a bit more logical, e.g. d_bw / decimation * 2 -> 2 steps down
                return i*decim_size;
            }
            //d_debug << "G:" << gradient[i] << std::endl;
        }

        return -1;
    }

    uint8_t decoder_impl::lookup_cr(uint8_t bytevalue) {
        switch (bytevalue & 0x0f) {
            case 0x01: {
                return 4;
                break;
            }
            case 0x0f: {
                return 3;
                break;
            }
            case 0x0d: {
                return 2;
                break;
            }
            case 0x0b: {
                return 1;
                break;
            }
            default: {
                return 4;
                break;
            }
        }
    }

    void decoder_impl::msg_raw_chirp_debug(const gr_complex* raw_samples, uint32_t num_samples) {
        pmt::pmt_t chirp_blob = pmt::make_blob(raw_samples, sizeof(gr_complex) * num_samples);
        message_port_pub(pmt::mp("debug"), chirp_blob);
    }

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
                    }
                }
                consume_each(2*d_samples_per_symbol);
                break;
            }
            case SYNC: {
                double c = detect_downchirp(&input[0], d_samples_per_symbol);
                d_debug << "Cd: " << c << std::endl;

                if(c > 0.98f) {
                    d_debug << "SYNC: " << c << std::endl;
                    // Debug stuff
                    samples_to_file("/tmp/sync", &input[0], d_samples_per_symbol, sizeof(gr_complex));

                    d_state = PAUSE;
                    consume_each(d_samples_per_symbol);
                } else {
                    d_corr_fails++;
                    if(d_corr_fails > 32) {
                        d_state = DETECT;
                        d_debug << "Lost sync" << std::endl;
                    }
                    consume_each(d_samples_per_symbol);
                }
                break;
            }
            case PAUSE: {
                d_state = DECODE_HEADER;

                //samples_debug(input, d_samples_per_symbol + d_delay_after_sync);
                consume_each(d_samples_per_symbol + d_delay_after_sync);
                break;
            }
            case DECODE_HEADER: {
                d_cr = 4;
                if(demodulate(input, true)) {
                    uint8_t decoded[3];
                    d_payload_length = 3; // TODO: A bit messy. I think it's better to make an internal decoded std::vector
                    decode(decoded, true);

                    nibble_reverse(decoded, 1); // TODO: Why? Endianess?
                    d_payload_length = decoded[0];
                    d_cr = lookup_cr(decoded[1]);

                    int symbols_per_block = d_cr + 4;
                    int bits_needed = ((d_payload_length * 8) + 16);
                    float symbols_needed = float(bits_needed) * (symbols_per_block / 4.0f) / float(d_sf);
                    int blocks_needed = ceil(symbols_needed / symbols_per_block);
                    d_payload_symbols = blocks_needed * symbols_per_block;

                    d_debug << "LEN: " << d_payload_length << " (" << d_payload_symbols << " symbols)" << std::endl;

                    d_state = DECODE_PAYLOAD;
                }

                msg_raw_chirp_debug(raw_input, d_samples_per_symbol);
                //samples_debug(input, d_samples_per_symbol);
                consume_each(d_samples_per_symbol);
                break;
            }
            case DECODE_PAYLOAD: {
                if(demodulate(input, false)) {
                    d_payload_symbols -= (4 + d_cr);

                    if(d_payload_symbols <= 0) {
                        uint8_t decoded[d_payload_length];
                        memset(decoded, 0x00, d_payload_length);
                        decode(decoded, false);

                        d_state = DETECT;
                        d_data.clear();
                    }
                }

                msg_raw_chirp_debug(raw_input, d_samples_per_symbol);
                //samples_debug(input, d_samples_per_symbol);
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

    void decoder_impl::set_sf(uint8_t sf) {
        if(sf >= 7 && sf <= 13)
            d_sf = sf;
    }

    void decoder_impl::set_samp_rate(float samp_rate) {
        d_samples_per_second = samp_rate;
    }

  } /* namespace lora */
} /* namespace gr */
