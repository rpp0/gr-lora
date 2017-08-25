/* -*- c++ -*- */
/*
 * Copyright 2017 Pieter Robyns.
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
#include "controller_impl.h"
#include "channelizer_impl.h"

namespace gr {
  namespace lora {

    controller::sptr
    controller::make(void* parent) {
      return gnuradio::get_initial_sptr
        (new controller_impl(parent));
    }

    /*
     * The private constructor
     */
    controller_impl::controller_impl(void* parent)
      : gr::block("controller",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0)) {
        d_parent = parent;
        d_port = pmt::intern("control");
        message_port_register_in(d_port);
        set_msg_handler(d_port, boost::bind(&controller_impl::handle_control, this, _1));
    }

    void controller_impl::handle_control(pmt::pmt_t msg){
        if(pmt::symbol_to_string(pmt::car(msg)).compare("cfo") == 0) {
            std::cout << "Setting CFO " << pmt::to_double(pmt::cdr(msg)) << std::endl;
            ((channelizer_impl*)d_parent)->apply_cfo(pmt::to_double(pmt::cdr(msg))); // TODO: Pretty hacky cast, can we do this in a cleaner way?
        }
    }

    /*
     * Our virtual destructor.
     */
    controller_impl::~controller_impl()
    {
    }


  } /* namespace lora */
} /* namespace gr */
