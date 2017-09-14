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

#include <stdint.h>

typedef enum cr { CR1=1, CR2, CR3, CR4 } cr_t;

typedef struct __attribute__((__packed__)) loraphy_header {
    uint8_t length;
    uint8_t crc_msn : 4;
    uint8_t has_mac_crc : 1;
    uint8_t cr : 3;
    uint8_t crc_lsn : 4;
    uint8_t reserved : 4;
} loraphy_header_t;
