/* -*- c++ -*- */
/*
 * Copyright 2017 Pieter Robyns, William Thenaers.
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

#include <lora/loratap.h>
#include <lora/loraphy.h>
#include <gnuradio/io_signature.h>
#include <lora/utilities.h>
#include "message_socket_sink_impl.h"

namespace gr {
    namespace lora {

        message_socket_sink::sptr message_socket_sink::make(std::string ip, int port, bool loratap, bool loraphy) {
            return gnuradio::get_initial_sptr(new message_socket_sink_impl(ip, port, loratap, loraphy));
        }

        /**
         *  \brief The private constructor
         *
         *      Create a UDP socket connection to send the data through.
         */
        message_socket_sink_impl::message_socket_sink_impl(std::string ip, int port, bool loratap, bool loraphy)
            : gr::block("message_socket_sink", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0)),
            d_ip(ip),
            d_port(port),
            d_loratap(loratap),
            d_loraphy(loraphy) {

            message_port_register_in(pmt::mp("in"));
            set_msg_handler(pmt::mp("in"), boost::bind(&message_socket_sink_impl::handle, this, _1));

            d_socket = socket(AF_INET, SOCK_DGRAM, 0);

            if(d_socket < 0) {
                perror("[message_socket_sink] Failed to create socket!");
                exit(EXIT_FAILURE);
            }

            d_sock_addr                   = new struct sockaddr_in;
            d_sock_addr->sin_family       = AF_INET;
            d_sock_addr->sin_addr.s_addr  = htonl(INADDR_ANY);
            d_sock_addr->sin_port         = htons(0);    // Source port: 0 is any

            if(bind(d_socket, (const struct sockaddr*) d_sock_addr, sizeof(*d_sock_addr)) < 0) {
                perror("[message_socket_sink] Socket bind failed!");
                exit(EXIT_FAILURE);
            }

            d_sock_addr->sin_port = htons(d_port);
            // IP string to int conversion
            inet_pton(AF_INET, d_ip.c_str(), &d_sock_addr->sin_addr.s_addr);
        }

        /**
         *  \brief  Our virtual destructor.
         */
        message_socket_sink_impl::~message_socket_sink_impl() {
            delete d_sock_addr;
            shutdown(d_socket, 1);      // close transmissions
        }

        /**
         *  \brief  Handle a message and send its contents through an UDP packet to the loopback interface.
         */
        void message_socket_sink_impl::handle(pmt::pmt_t msg) {
            uint8_t* data = (uint8_t*)pmt::blob_data(msg);
            uint32_t offset = 0;
            size_t size = pmt::blob_length(msg);
            loratap_header_t *loratap_header;
            loraphy_header_t *loraphy_header;
            uint8_t *payload;

            offset = gr::lora::dissect_packet((const void **)&loratap_header, sizeof(loratap_header_t), data, offset);
            offset = gr::lora::dissect_packet((const void **)&loraphy_header, sizeof(loraphy_header_t), data, offset);
            uint32_t payload_length = loraphy_header->length + (MAC_CRC_SIZE * loraphy_header->has_mac_crc);
            offset = gr::lora::dissect_packet((const void **)&payload, sizeof(uint8_t)*payload_length, data, offset);
            if(offset != size) {
                std::cerr << "message_socket_sink_impl::handle: invalid read: " << offset << " != " << size << std::endl;
                exit(EXIT_FAILURE);
            }

            if(d_loratap == false) {
                msg_send_udp(loraphy_header, payload, payload_length); // Send message over UDP socket
            } else {
                //msg_send_loratap(); // Send message over raw LoRa socket
                std::cerr << "message_socket_sink_impl::handle: LoRaTap is not supported yet." << std::endl;
                return;
            }
        }

        void message_socket_sink_impl::msg_send_udp(const loraphy_header_t* loraphy_header, const uint8_t* payload, const uint32_t payload_length) {
            bool error = false;

            if(d_loraphy) {
                int32_t msg_len = sizeof(loraphy_header_t) + payload_length;
                if (sendto(d_socket, loraphy_header, msg_len, 0, (const struct sockaddr*)d_sock_addr, sizeof(*d_sock_addr)) != msg_len)
                    error = true;
            } else {
                int32_t msg_len = payload_length-(MAC_CRC_SIZE * loraphy_header->has_mac_crc); // User did not request PHY header
                if (sendto(d_socket, payload, msg_len, 0, (const struct sockaddr*)d_sock_addr, sizeof(*d_sock_addr)) != msg_len)
                    error = true;
            }

            if(error) {
                perror("message_socket_sink_impl::handle: mismatch in number of bytes sent");
                exit(EXIT_FAILURE);
            }
        }

    } /* namespace lora */
} /* namespace gr */
