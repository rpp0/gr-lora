#!/usr/bin/sh
export VOLK_GENERIC=1
export GR_DONT_LOAD_PREFS=1
export srcdir=../python
export GR_CONF_CONTROLPORT_ON=False
export PATH=../build/python:$PATH
export LD_LIBRARY_PATH=../build/lib:$LD_LIBRARY_PATH
export PYTHONPATH=../build/swig:$PYTHONPATH
/usr/bin/python2 ./_examplify.py
