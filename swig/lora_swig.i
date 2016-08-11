/* -*- c++ -*- */

#define LORA_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "lora_swig_doc.i"

%{
#include "lora/decoder.h"
%}


%include "lora/decoder.h"
GR_SWIG_BLOCK_MAGIC2(lora, decoder);
