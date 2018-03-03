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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "message_socket_source_impl.h"

namespace gr
{
  namespace lora
  {

    message_socket_source::sptr
    message_socket_source::make (const std::string& addr, uint16_t port,
				 size_t mtu)
    {
      return gnuradio::get_initial_sptr (
	  new message_socket_source_impl (addr, port, mtu));
    }

    /*
     * The private constructor
     */
    message_socket_source_impl::message_socket_source_impl (
	const std::string& addr, uint16_t port, size_t mtu) :
	    gr::block ("message_socket_source",
		       gr::io_signature::make (0, 0, 0),
		       gr::io_signature::make (0, 0, 0)),
	    d_addr (addr),
	    d_udp_port (port),
	    d_mtu (mtu)
    {
    }

    /*
     * Our virtual destructor.
     */
    message_socket_source_impl::~message_socket_source_impl ()
    {
    }

  } /* namespace lora */
} /* namespace gr */

