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

#ifndef INCLUDED_LORA_MESSAGE_SOCKET_SINK_IMPL_H
#define INCLUDED_LORA_MESSAGE_SOCKET_SINK_IMPL_H

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <lora/message_socket_sink.h>

namespace gr {
    namespace lora {

        class message_socket_sink_impl : public message_socket_sink {
            private:
                const std::string d_host = "127.0.0.1";
                const int d_port         = 40868;

                // socket
                struct sockaddr_in *d_sock_addr;
                int d_socket;

                void handle(pmt::pmt_t msg);

            public:
                message_socket_sink_impl();

                ~message_socket_sink_impl();
        };

    } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_MESSAGE_SOCKET_SINK_IMPL_H */
