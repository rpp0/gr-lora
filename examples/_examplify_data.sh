#!/usr/bin/sh
export VOLK_GENERIC=1
export GR_DONT_LOAD_PREFS=1
export srcdir=/home/william/gr-lora/python
export GR_CONF_CONTROLPORT_ON=False
export PATH=/home/william/gr-lora/build/python:$PATH
export LD_LIBRARY_PATH=/home/william/gr-lora/build/lib:$LD_LIBRARY_PATH
export PYTHONPATH=/home/william/gr-lora/build/swig:$PYTHONPATH
/usr/bin/python2 /home/william/lora-samples/_examplify.py
