from archlinux:latest

workdir /lib

# Update to latest arch
run pacman -Syu --noconfirm

# Install required dependencies
run pacman -S git python2-scipy swig cppunit fftw boost boost-libs gnuradio gnuradio-osmosdr libvolk log4cpp base-devel cmake wxgtk-common wxgtk2 wxgtk3 wxpython libuhd-firmware gnuradio-companion python2-requests --noconfirm

workdir /liquid

# Manual liquid-dsp install
run git clone https://github.com/jgaeddert/liquid-dsp.git . && \
    sh ./bootstrap.sh && \
    sh ./configure --prefix=/usr && \
    make && \
    make install

# Install gr-lora
workdir /src

arg CACHEBUST
run git clone https://github.com/rpp0/gr-lora.git . && \
    mkdir build && \
    cd build && \
    cmake ../ -DCMAKE_INSTALL_PREFIX=/usr && \
    make && \
    make install && \
    ldconfig

workdir /src/apps

expose 40868
