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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/expj.h>
#include <lora/utilities.h>
#include "encoder_impl.h"
#include "tables.h"

namespace gr {
  namespace lora {

    encoder::sptr
    encoder::make() {
      return gnuradio::get_initial_sptr
        (new encoder_impl());
    }

    /*
     * The private constructor
     */
    encoder_impl::encoder_impl() : gr::sync_block("encoder",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(1, 1, sizeof(gr_complex))) {
        // Bind ports
        message_port_register_in(pmt::mp("in"));
        set_msg_handler(pmt::mp("in"), boost::bind(&encoder_impl::handle_loratap, this, _1));

        set_output_multiple(pow(2,16));

        // Initialize variables
        d_osr = 8;
        d_samples_per_second = 125000*d_osr;
        d_num_preamble_symbols = 8;
        d_bw = 125000;
        d_explicit = true;
        d_reduced_rate = false;
        d_chirp_phi0 = -M_PI;

        d_dt = 1.0f / d_samples_per_second;
        d_sample_buffer.reserve(d_samples_per_second); // Allocate for one second of samples

        // Setup hamming coding schemes
        fec_scheme fs = LIQUID_FEC_HAMMING84;
        d_h48_fec = fec_create(fs, NULL);

        // Setup chirp lookup tables TODO unused now
        for(uint8_t sf = 6; sf <= 12; sf++) {
            const uint32_t chips_per_symbol = pow(2, sf);
            const double symbols_per_second = (double)d_bw / chips_per_symbol;
            const double samples_per_symbol = d_samples_per_second / symbols_per_second;
            const double T = 0.5 * d_bw * symbols_per_second;
            const double f0 = -(d_bw / 2.0);
            const double pre_dir = 2.0 * M_PI;
            double t;

            std::vector<gr_complex> chirp(samples_per_symbol*2);
            for (uint32_t i = 0u; i < samples_per_symbol; i++) {
                t = d_dt * i;
                gr_complex sample = gr_expj(pre_dir * t * (f0 + T * t));
                chirp[i] = sample;
                chirp[i+samples_per_symbol] = sample;
            }

            std::cout << "SF " << (int)sf << " has " << samples_per_symbol << " samples" << std::endl;

            d_chirps[sf] = chirp; // Copy vector metadata to chirps hashmap. Note that vector internally allocates on heap.
        }
    }

    void encoder_impl::transmit_chirp(bool up, uint8_t sf, uint16_t symbol, bool quarter = false) {
        const uint32_t chips_per_symbol = pow(2, sf);
        const double symbols_per_second = (double)d_bw / chips_per_symbol;
        const uint32_t samples_per_symbol = d_samples_per_second / symbols_per_second;
        const double T = 0.5 * d_bw * symbols_per_second;
        const double f0 = -(d_bw / 2.0);
        double pre_dir = 2.0 * M_PI;
        double t;

        if(!up)
            pre_dir *= -1;

        std::vector<gr_complex> chirp(samples_per_symbol);
        double phase = 0;
        for (uint32_t i = 0u; i < samples_per_symbol; i++) {
            t = d_dt * ((i + (d_osr * symbol)) % samples_per_symbol);
            phase = d_chirp_phi0 + (pre_dir * t * (f0 + T * t));
            chirp[i] = gr_expj(phase);
        }

        // Add chirp to buffer
        if(quarter)
            d_sample_buffer.insert(d_sample_buffer.end(), chirp.begin(), chirp.begin() + chirp.size() / 4);
        else
            d_sample_buffer.insert(d_sample_buffer.end(), chirp.begin(), chirp.end());

        // Set phase
        d_chirp_phi0 = phase;
    }

    /*
     * Our virtual destructor.
     */
    encoder_impl::~encoder_impl() {
        fec_destroy(d_h48_fec);
    }

    void encoder_impl::handle_loratap(pmt::pmt_t msg) {
        gr::thread::scoped_lock guard(d_mutex); // Auto lock & unlock (RAII)

        d_packets.push_back(msg);
    }

    bool encoder_impl::parse_packet_conf(loraconf_t& conf, uint8_t* packet, uint32_t packet_len) {
        if(packet_len <= sizeof(loraconf_t)) {
            return false;
        }

        memcpy(&conf.tap, packet, sizeof(loratap_header_t));
        memcpy(&conf.phy, packet + sizeof(loratap_header_t), sizeof(loraphy_header_t));

        return true;
    }

    /**
     * Get packets from queue and initialize transmission.
     */
    int encoder_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items) {
        gr_complex* out = (gr_complex*)output_items[0];

        // Temporary         ve  pa      le              fq  bw  sf  pr  mr  cr  sn  sy  H1  H1  H1
        char test_pkt[] = "\x00\x00\x12\x00\x00\xa1\xbc\x33\x01\x07\x00\x00\x00\x00\x12\x17\x91\xa0\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x20\x21\x22\xb8\x73";
        loraconf_t conf;
        memset(&conf, 0, sizeof(loraconf_t));

        if(!parse_packet_conf(conf, (uint8_t*)test_pkt, sizeof(test_pkt))) {
            std::cerr << "Malformed LoRa packet received" << std::endl;
            exit(1);
        }

        print_loraconf(conf);
        transmit_packet(conf, (uint8_t*)(test_pkt + sizeof(loratap_header_t)));

        if(d_sample_buffer.size() >= noutput_items) {
            // Get noutput_items from buffer
            memcpy(out, &d_sample_buffer[0], noutput_items * sizeof(gr_complex));
            d_sample_buffer.erase(d_sample_buffer.begin(), d_sample_buffer.begin() + noutput_items);

            return noutput_items;
        } else {
            return 0; // Wait for more symbols
        }
        // -------------------------------------------------------------------------------------

        // Get packet from queue
        gr::thread::scoped_lock guard(d_mutex);
        if(d_packets.size() > 0) {
            pmt::pmt_t packet = d_packets.front();

            // Process
            std::cout << "Processing packet" << std::endl;
            //uint8_t* packet = (uint8_t*)pmt::blob_data(msg);
            //size_t packet_length = pmt::blob_length(msg);

            // Delete from queue
            d_packets.erase(d_packets.begin());
        }

        // Tell runtime system how many output items we produced.
        return 0;
    }

    void encoder_impl::shuffle(uint8_t *data, uint32_t data_len, const uint8_t *shuffle_pattern) {
        for (uint32_t i = 0u; i < data_len; i++) {
            uint8_t result = 0u;

            for (uint32_t j = 0u; j < 8; j++) {
                result |= !!(data[i] & (1u << shuffle_pattern[j])) << j;
            }

            data[i] = result;
        }
    }

    uint32_t encoder_impl::interleave_block(uint16_t *symbols, uint8_t* data, uint8_t sf, uint8_t cr, bool reduced_rate) {
        if(reduced_rate)
            sf -= 2;

        // Determine symbols for this block
        for(uint8_t symbol = 0; symbol < 4+cr; symbol++) {
            for(int8_t bit = sf-1; bit >= 0; bit--) {
                symbols[symbol] |= ((data[bit] >> symbol) & 0x01) << bit;
            }

            int32_t to_rotate = sf-symbol;
            if(to_rotate < 0)
                to_rotate += sf;

            symbols[symbol] = gr::lora::rotl(symbols[symbol], to_rotate, sf);
        }

        // Rotate to interleave
        std::vector<uint16_t> symbols_v(symbols, symbols + (4+cr));
        print_interleave_matrix(std::cout, symbols_v, sf);
        print_vector(std::cout, symbols, "Chips", 4+cr, sf);

        // Determine bins
        std::cout << "Bins: ";
        for(uint8_t symbol = 0; symbol < 4+cr; symbol++) {
            symbols[symbol] = gray_decode(symbols[symbol]);
            if(reduced_rate)
                symbols[symbol] <<= 2;
            std::cout << (int)symbols[symbol] << ", ";
        }
        std::cout << std::endl;

        return sf;
    }

    void encoder_impl::nibble_swap(uint8_t* encoded, uint32_t length) {
        for(uint32_t i = 0; i+1 < length; i += 2) {
            uint8_t tmp = encoded[i];
            encoded[i] = encoded[i+1];
            encoded[i+1] = tmp;
        }
    }

    void encoder_impl::transmit_packet(loraconf_t& conf, uint8_t* packet) { // TODO: clean up
        uint32_t packet_length = conf.phy.length + sizeof(loraphy_header_t); //
        uint32_t num_bytes = packet_length*2;
        uint8_t encoded[num_bytes];
        uint32_t num_symbols = num_bytes * ((4.0+conf.phy.cr) / conf.tap.channel.sf) + 0.5;
        uint32_t encoded_offset = 0;
        uint32_t packet_offset = 0;

        // Add preamble symbols to queue
        for(uint32_t i = 0; i < d_num_preamble_symbols; i++) {
            transmit_chirp(true, conf.tap.channel.sf, 0);
        }

        // Add sync words to queue
        uint8_t sync_word = 0x12;
        uint32_t sync_offset_1 = ((sync_word & 0xf0) >> 4) * pow(2, conf.tap.channel.sf) * d_osr / 32;
        uint32_t sync_offset_2 = (sync_word & 0x0f) * pow(2, conf.tap.channel.sf) * d_osr / 32;
        transmit_chirp(true, conf.tap.channel.sf, sync_offset_1);
        transmit_chirp(true, conf.tap.channel.sf, sync_offset_2);

        // Add SFD to queue
        transmit_chirp(false, conf.tap.channel.sf, 0);
        transmit_chirp(false, conf.tap.channel.sf, 0);
        transmit_chirp(false, conf.tap.channel.sf, 0, true);

        // If explicit header, add one block (= SF codewords) to queue in reduced rate mode (and always 4/8)
        if(d_explicit) {
            fec_encode(d_h48_fec, 3, packet, encoded); // Header is always 4/8
            packet_offset = 3;
            encoded_offset = 5;
        }

        // Add remaining blocks to queue
        print_vector(std::cout, packet, "Packet", packet_length, 8);

        fec_encode(d_h48_fec, packet_length - packet_offset, packet+packet_offset, encoded+encoded_offset); // TODO: change to appropriate scheme
        nibble_swap(encoded+encoded_offset, num_bytes-encoded_offset);
        print_vector(std::cout, encoded, "Encoded", num_bytes, 8);

        whiten(encoded+encoded_offset, gr::lora::prng_payload, num_bytes-encoded_offset);
        print_vector(std::cout, encoded, "Whitened", num_bytes, 8);

        const uint8_t shuffle_pattern[] = {1, 2, 3, 5, 4, 0, 6, 7};
        shuffle(encoded, num_bytes, shuffle_pattern);
        print_vector(std::cout, encoded, "Shuffled", num_bytes, 8);

        // Interleaving
        uint16_t symbols[num_symbols];
        memset(symbols, 0x00, num_symbols * sizeof(uint16_t));
        uint32_t symbols_done = 0;
        uint32_t interleave_offset = 0;

        if(d_explicit) {
            interleave_offset += interleave_block(symbols, encoded, conf.tap.channel.sf, 4, true);
            symbols_done += 8;
        }

        while(interleave_offset+conf.tap.channel.sf <= num_bytes) { // TODO: needs to be exact number of bytes
            interleave_offset += interleave_block(symbols+symbols_done, encoded+interleave_offset, conf.tap.channel.sf, conf.phy.cr, d_reduced_rate);
            symbols_done += 4 + conf.phy.cr;
        }

        // Transmission
        for(uint32_t i = 0; i < num_symbols; i++) {
            transmit_chirp(true, conf.tap.channel.sf, symbols[i]);
        }
    }

  } /* namespace lora */
} /* namespace gr */
