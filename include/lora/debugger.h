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

#ifndef INCLUDED_DEBUGGER_H
#define INCLUDED_DEBUGGER_H

#include <gnuradio/gr_complex.h>
#include <vector>
#include <stdint.h>

#define PACKED __attribute__((packed, aligned(1)))

namespace gr {
    namespace lora {
        class debugger {
        public:
            debugger();
            virtual ~debugger();

            void attach(std::string path = "/tmp/gr_lora.sock");
            void detach(void);
            void analyze_samples(bool clear, bool draw_over);
            void store_samples(const gr_complex* samples, uint32_t length);
        private:
            typedef struct header {
                uint32_t length;
                bool draw_over;
            } PACKED header;

            void send_samples();
            std::vector<gr_complex> d_samples;
            int d_socket;
            bool d_attached;
        };
    }
}

#endif /* INCLUDED_DEBUGGER_H */
