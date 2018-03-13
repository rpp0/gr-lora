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

#include <lora/loratap.h>
#include <lora/loraphy.h>
#include <gnuradio/io_signature.h>
#include "message_socket_source_impl.h"

namespace gr
{
  namespace lora
  {

    message_socket_source::sptr
    message_socket_source::make (const std::string& addr, uint16_t port,
				 size_t mtu, size_t payload_len)
    {
      return gnuradio::get_initial_sptr (
	  new message_socket_source_impl (addr, port, mtu, payload_len));
    }

    /*
     * The private constructor
     */
    message_socket_source_impl::message_socket_source_impl (
	const std::string& addr, uint16_t port, size_t mtu, size_t payload_len) :
	    gr::block ("message_socket_source",
		       gr::io_signature::make (0, 0, 0),
		       gr::io_signature::make (0, 0, 0)),
	    d_addr (addr),
	    d_udp_port (port),
	    d_mtu (mtu),
	    d_payload_len (payload_len),
	    d_running (true),
	    d_layer (0)
    {
      message_port_register_out (pmt::mp ("out"));
      boost::shared_ptr<boost::thread> (
	  new boost::thread (
	      boost::bind (&message_socket_source_impl::msg_receive_udp,
			   this)));

    }

    void
    message_socket_source_impl::msg_receive_udp ()
    {
      int sock;
      struct sockaddr_in sin;
      struct sockaddr client_addr;
      socklen_t client_addr_len;
      ssize_t ret;
      uint8_t *buf;
      uint32_t bytes_num;

      if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
	perror ("opening UDP socket");
	exit (EXIT_FAILURE);
      }

      memset (&client_addr, 0, sizeof(struct sockaddr));
      memset (&sin, 0, sizeof(struct sockaddr_in));
      sin.sin_family = AF_INET;
      sin.sin_port = htons (d_udp_port);

      if (inet_aton (d_addr.c_str (), &(sin.sin_addr)) == 0) {
	printf ("Wrong IP address\n");
	close (sock);
	exit (EXIT_FAILURE);
      }

      if (bind (sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))
	  == -1) {
	perror ("UDP bind");
	close (sock);
	exit (EXIT_FAILURE);
      }

      /* All good until now. Allocate buffer memory and proceed */
      buf = new uint8_t[d_mtu];

      while (d_running) {
	ret = recvfrom (sock, buf, d_mtu, 0, &client_addr, &client_addr_len);
	if (ret > 0) {
	  bytes_num = (uint32_t) ret;
	  if (bytes_num == (sizeof(loratap_header_t)+sizeof(loraphy_header_t)+ d_payload_len)) {
	    pmt::pmt_t lora_tap;
	    lora_tap = pmt::make_blob (buf, bytes_num);
	    message_port_pub (pmt::mp ("out"), lora_tap);
	  }

	}
	else {
	  perror ("UDP recvfrom");
	  close (sock);
	  delete[] buf;
	  exit (EXIT_FAILURE);
	}
      }
      close (sock);
      delete[] buf;
      exit (EXIT_SUCCESS);
    }

    /*
     * Our virtual destructor.
     */
    message_socket_source_impl::~message_socket_source_impl ()
    {
      d_running = false;
      d_thread->join ();
    }

  } /* namespace lora */
} /* namespace gr */

