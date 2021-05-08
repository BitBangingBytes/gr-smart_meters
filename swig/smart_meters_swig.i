/* -*- c++ -*- */

#define SMART_METERS_API

%include "gnuradio.i"           // the common stuff

//load generated python docstrings
%include "smart_meters_swig_doc.i"

%{
#include "smart_meters/GridStream.h"
%}

%include "smart_meters/GridStream.h"
GR_SWIG_BLOCK_MAGIC2(smart_meters, GridStream);
