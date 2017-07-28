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

#ifndef INCLUDED_LORA_CONTROLLER_IMPL_H
#define INCLUDED_LORA_CONTROLLER_IMPL_H

#include <lora/controller.h>

namespace gr {
  namespace lora {

    class controller_impl : public controller {
    private:
        void* d_parent;
        pmt::pmt_t d_port;

        void handle_control(pmt::pmt_t msg);

    public:
        controller_impl(void* parent);
        ~controller_impl();

        // Where all the action really happens
    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_CONTROLLER_IMPL_H */
