/* -*- c++ -*- */
/*
 * Copyright 2018 Nikos Karamolegkos.
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

#ifndef INCLUDED_LORA_MESSAGE_SOCKET_SOURCE_IMPL_H
#define INCLUDED_LORA_MESSAGE_SOCKET_SOURCE_IMPL_H

#include <lora/message_socket_source.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <boost/thread.hpp>

namespace gr
{
  namespace lora
  {

    class message_socket_source_impl : public message_socket_source {
    private:
        const std::string d_addr;
        const uint16_t d_udp_port;
        bool d_running;
        boost::shared_ptr<boost::thread> d_thread;

        void msg_receive_udp();

    public:
        message_socket_source_impl(const std::string& addr, uint16_t port);
        ~message_socket_source_impl();
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_MESSAGE_SOCKET_SOURCE_IMPL_H */
