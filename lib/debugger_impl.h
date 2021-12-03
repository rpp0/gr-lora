/* -*- c++ -*- */
/*
 * Copyright 2021 gr-lora rpp0.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_LORA_DEBUGGER_IMPL_H
#define INCLUDED_LORA_DEBUGGER_IMPL_H

#include <lora/debugger.h>

namespace gr {
  namespace lora {

    class debugger_impl : public debugger
    {
     private:
      // Nothing to declare in this block.

     public:
      debugger_impl();
      ~debugger_impl();

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);

    };

  } // namespace lora
} // namespace gr

#endif /* INCLUDED_LORA_DEBUGGER_IMPL_H */
