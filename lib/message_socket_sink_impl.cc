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

#include <gnuradio/io_signature.h>
#include "message_socket_sink_impl.h"

namespace gr {
    namespace lora {

        message_socket_sink::sptr message_socket_sink::make() {
            return gnuradio::get_initial_sptr(new message_socket_sink_impl());
        }

        /**
         *  \brief The private constructor
         *
         *      Create a UDP socket connection to send the data through.
         */
        message_socket_sink_impl::message_socket_sink_impl()
            : gr::block("message_socket_sink",
                        gr::io_signature::make(0, 0, 0),
                        gr::io_signature::make(0, 0, 0)) {
            message_port_register_in(pmt::mp("in"));
            set_msg_handler(pmt::mp("in"), boost::bind(&message_socket_sink_impl::handle, this, _1));

            d_socket = socket(AF_INET, SOCK_DGRAM, 0);

            if (d_socket < 0) {
                perror("[message_socket_sink] Failed to create socket!");
                exit(EXIT_FAILURE);
            }

            d_sock_addr                   = new struct sockaddr_in;
            d_sock_addr->sin_family       = AF_INET;
            d_sock_addr->sin_addr.s_addr  = htonl(INADDR_ANY);
            d_sock_addr->sin_port         = htons(0);    // Source port: 0 is any

            if (bind(d_socket,
                     (const struct sockaddr*) d_sock_addr,
                     sizeof(*d_sock_addr))
                < 0) {
                perror("[message_socket_sink] Socket bind failed!");
                exit(EXIT_FAILURE);
            }

            d_sock_addr->sin_port         = htons(this->d_port);
            // == "127.0.0.1" to int translation
            inet_pton(AF_INET, this->d_host.c_str(), &d_sock_addr->sin_addr.s_addr);
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
            uint8_t *data = (uint8_t*) pmt::blob_data(msg);
            size_t size = pmt::blob_length(msg);

            #ifdef DEBUG
                printf("Received message:\n\t");

                for (size_t i = 0; i < size; ++i)
                    printf("%02x ", data[i]);

                putchar('\n');
            #endif

            if (sendto(d_socket, data, size, 0,
                       (const struct sockaddr*) d_sock_addr,
                       sizeof(*d_sock_addr))
                != (ssize_t)size) {
                perror("[message_socket_sink] Mismatch in number of bytes sent");
                exit(EXIT_FAILURE);
            }
        }

    } /* namespace lora */
} /* namespace gr */
