/* -*- c++ -*- */

#define LORA_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "lora_swig_doc.i"

%{
#include "lora/decoder.h"
#include "lora/message_file_sink.h"
#include "lora/message_socket_sink.h"
#include "lora/channelizer.h"
#include "lora/controller.h"
#include "lora/message_socket_source.h"
%}


%include "lora/decoder.h"
GR_SWIG_BLOCK_MAGIC2(lora, decoder);
%include "lora/message_file_sink.h"
GR_SWIG_BLOCK_MAGIC2(lora, message_file_sink);
%include "lora/message_socket_sink.h"
GR_SWIG_BLOCK_MAGIC2(lora, message_socket_sink);
%include "lora/channelizer.h"
GR_SWIG_BLOCK_MAGIC2(lora, channelizer);
%include "lora/controller.h"
GR_SWIG_BLOCK_MAGIC2(lora, controller);
%include "lora/message_socket_source.h"
GR_SWIG_BLOCK_MAGIC2(lora, message_socket_source);
