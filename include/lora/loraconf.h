/* -*- c++ -*- */
/*
 * Copyright 2018 Pieter Robyns.
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

#ifndef LORACONF_H
#define LORACONF_H

#include <lora/loratap.h>
#include <lora/loraphy.h>

typedef struct __attribute__((__packed__)) loraconf {
    loratap_header_t tap;
    loraphy_header_t phy;
} loraconf_t;

void print_loraconf(loraconf_t& conf) {
    std::cout << "*** TAP" << std::endl;
    std::cout << "\tVersion: " << conf.tap.lt_version << std::endl;
    std::cout << "\tPadding: " << conf.tap.lt_padding << std::endl;
    std::cout << "\tLength: " << conf.tap.lt_length << std::endl;

    std::cout << "\tFrequency: " << conf.tap.channel.frequency << std::endl;
    std::cout << "\tBandwidth: " << conf.tap.channel.bandwidth << std::endl;
    std::cout << "\tSF: " << conf.tap.channel.sf << std::endl;

    std::cout << "\tPacket RSSI: " << conf.tap.rssi.packet_rssi << std::endl;
    std::cout << "\tMax RSSI: " << conf.tap.rssi.max_rssi << std::endl;
    std::cout << "\tCurrent RSSI: " << conf.tap.rssi.current_rssi << std::endl;
    std::cout << "\tSNR: " << conf.tap.rssi.snr << std::endl;

    std::cout << "\tSync: " << conf.tap.sync_word << std::endl;

    std::cout << "*** PHY" << std::endl;
    std::cout << "\tLength: " << conf.phy.length << std::endl;
    std::cout << "\tCR: " << conf.phy.cr << std::endl;
    std::cout << "\tHas CRC: " << (bool)conf.phy.has_mac_crc << std::endl;
    uint8_t crc = (conf.phy.crc_msn << 4) | conf.phy.crc_lsn;
    std::cout << "\tCRC: " << crc << std::endl;
    std::cout << "\tReserved: " << conf.phy.reserved << std::endl;
}

#endif
