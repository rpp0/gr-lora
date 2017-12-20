/* -*- c++ -*- */
/*
 * Copyright 2017 Pieter Robyns, William Thenaers.
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

#ifndef UTILITIES_H
#define UTILITIES_H

#include <cstdint>
#include <string.h>
#include <iomanip>

#define MAC_CRC_SIZE 2u
#define MAX_PWR_QUEUE_SIZE 4
#define SM(value, shift, mask) (((value) << (shift)) & (mask))
#define MS(value, mask, shift) (((value) & (mask)) >> (shift))

namespace gr {
    namespace lora {
        static std::vector<std::string> term_colors = {
            "\e[0m",
            "\e[1m\e[91m",
            "\e[1m\e[92m",
            "\e[1m\e[93m",
            "\e[1m\e[94m",
            "\e[1m\e[95m",
            "\e[1m\e[96m",
            "\e[1m\e[97m",
            "\e[1m\e[31m",
            "\e[1m\e[32m",
            "\e[1m\e[33m",
            "\e[1m\e[34m",
            "\e[1m\e[35m",
            "\e[1m\e[36m"
        };

        /**
         *  \brief  Wrap indices Python-like, i.e. array[wrap_index(-1, array_length)] gets the last element.
         *
         *  \param  i
         *          Index of array
         *  \param  n
         *          Length of array
         */
        inline int32_t wrap_index(int32_t i, int32_t n) {
            return ((i % n) + n) % n;
        }

        /**
         *  \brief  Clamp given value in the given range.
         *
         *  \tparam T
         *          The type of variable to clamp.
         *          <br>`d`, `min` and `max` must be of this type.
         *  \param  d
         *          The value to clamp.
         *  \param  min
         *          The lower bound of the range.
         *  \param  max
         *          The upper bound of the range.
         */
        template <class T>
        inline T clamp(const T d, const T min, const T max) {
            const T t = d < min ? min : d;
            return t > max ? max : t;
        }

        /**
         *  \brief  Rotate the given bits to the left and return the result.
         *
         *  \param  bits
         *          The value to rotate.
         *  \param  count
         *          The amount of bits to rotate (shift to left and add to right).
         *  \param  size
         *          The size in bits used in `bits`.
         *          <BR>e.g. 1 byte given       => size = 8
         *          <BR>e.g. only 6 bits in use => size = 6, and all bits higher than (1 << size-1) will be zeroed.
         */
        inline uint32_t rotl(uint32_t bits, uint32_t count = 1u, const uint32_t size = 8u) {
            const uint32_t len_mask = (1u << size) - 1u;

            count %= size;      // Limit bit rotate count to size
            bits  &= len_mask;  // Limit given bits to size

            return ((bits << count) & len_mask) | (bits >> (size - count));
        }

        /**
         *  \brief  Return the `v` represented in a binary string.
         *
         *  \tparam T
         *          The type of variable to convert.
         *  \param  v
         *          The value to convert.
         *  \param  bitwidth
         *          The length in bits of the given variable `v`.
         */
        template <typename T>
        inline std::string to_bin(const T v, const uint32_t bitwidth) {
            #ifdef LSB_FIRST
                const uint64_t maxpow = bitwidth ? (1ull << (bitwidth - 1)) : 0;
                uint64_t mask;

                std::string result = "";

                for (mask = 0x1; mask <= maxpow; mask <<= 1) {
                    result += (v & mask) ? "1" : "0";
                }
            #else
                uint64_t mask = bitwidth ? (1ull << bitwidth) : 0;

                std::string result = "";

                while(mask >>= 1) {
                    result += (v & mask) ? "1" : "0";
                }
            #endif

            return result;
        }

        /**
         *  \brief  Append the data in a given vector to an output stream with a comma delimiter.
         *
         *  \tparam T
         *          The type of variable to append.
         *  \param  out
         *          The output stream to append to.
         *  \param  v
         *          The vector containing the data to append.
         *  \param  prefix
         *          A prefix to include before appending the data.
         *  \param  element_len_bits
         *          The length in bits of the data in `v`.
         */
        template <typename T>
        inline void print_vector_bin(std::ostream& out, const std::vector<T>& v, const std::string& prefix, const int element_len_bits) {
            out << prefix << ": ";

            for (const T& x : v)
                out << to_bin(x, element_len_bits) << ", ";

            out << std::endl << std::flush;
        }

        /**
         *  \brief  Check whether the parity of the given binary string is even.
         *
         *  \param  word
         *          The string to check.
         *  \param  even
         *          Check for even (`true`) or uneven (`false`) parity.
         */
        inline bool check_parity_string(const std::string& word, const bool even = true) {
            size_t count = 0, i = 0;

            while(i < 7) {
                if (word[i++] == '1')
                    ++count;
            }

            return (count & 0x1) == !even;
        }

        /**
         *  \brief  Check whether the parity of the given uint64_t is even.
         *          <BR>See https://graphics.stanford.edu/~seander/bithacks.html for more.
         *
         *  \param  word
         *          The uint64_t to check.
         *  \param  even
         *          Check for even (`true`) or uneven (`false`) parity.
         */
        inline bool check_parity(uint64_t word, const bool even = true) {
            word ^= word >> 1;
            word ^= word >> 2;
            word = (word & 0x1111111111111111UL) * 0x1111111111111111UL;

            return ((word >> 60ull) & 1ull) == !even;
        }

        /**
         *  \brief  Select the bits in data given by the indices in `*indices`.
         *
         *  \param  data
         *          The data to select bits from.
         *  \param  *indices
         *          Array with the indices to select.
         *  \param  n
         *          The amount of indices.
         */
        inline uint32_t select_bits(const uint32_t data, const uint8_t *indices, const uint8_t n) {
            uint32_t r = 0u;

            for(uint8_t i = 0u; i < n; ++i)
                r |= (data & (1u << indices[i])) ? (1u << i) : 0u;

            return r;
        }

        /**
         *  \brief  Select a single bit from the given byte.
         *
         *  \param  v
         *          The byte to select from.
         *  \param  i
         *          The index to select the bit from starting from the LSB.
         */
        inline uint8_t bit(const uint8_t v, const uint8_t i) {
            return ((v >> i) & 0x01);
        }

        /**
         *  \brief  Pack the given 8 bits in a byte with: `hgfe dcba`
         *
         *  \param  a-h
         *          The bits to pack with the LSB first.
         */
        inline uint8_t pack_byte(const uint8_t a, const uint8_t b, const uint8_t c, const uint8_t d,
                                 const uint8_t e, const uint8_t f, const uint8_t g, const uint8_t h) {
            return a | (b << 1) | (c << 2) | (d << 3) | (e << 4) | (f << 5) | (g << 6) | (h << 7);
        }

        /**
         *  \brief  Pack the given 4 bits in a nibble with: `dcba`
         *
         *  \param  a-d
         *          The bits to pack with the LSB first.
         */
        inline uint8_t pack_nibble(const uint8_t a, const uint8_t b, const uint8_t c, const uint8_t d) {
            return a | (b << 1) | (c << 2) | (d << 3);
        }

        /**
         *  \brief  Encode the given word with standard Hamming(7,4) and return a byte with the set parity bits.
         *
         *  \param  v
         *          The nibble to encode.
         */
        inline uint8_t hamming_encode_soft(const uint8_t v) {
            const uint8_t p1 = bit(v, 1) ^ bit(v, 2) ^ bit(v, 3);
            const uint8_t p2 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 2);
            const uint8_t p3 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 3);
            const uint8_t p4 = bit(v, 0) ^ bit(v, 2) ^ bit(v, 3);

            return pack_byte(p1, bit(v, 0), bit(v, 1), bit(v, 2), p2, bit(v, 3), p3, p4);
        }

        /**
         *  \brief  Swap nibbles of a byte array.
         *
         *  \param  array
         *          Array of uint8_t bytes
         *  \param  length
         *          Length of the array
         */
        inline void swap_nibbles(uint8_t* array, uint32_t length) {
            for(uint32_t i = 0; i < length; i++) {
                array[i] = ((array[i] & 0x0f) << 4) | ((array[i] & 0xf0) >> 4);
            }
        }

        /**
         *  DEPRECATED
         *  \brief  Hamming(8,4) decoding by constructing a Syndrome matrix LUT for XORing on parity errors.
         *
         *  \param  v
         *          The byte to decode.
         *  \return Returs a nibble containing the corrected data.
         */
        static inline uint8_t hamming_decode_soft_byte(uint8_t v) {
            // Precalculation
            // Which bits are covered (including self)?
            // p1 10110100
            // p2 01111000
            // p3 01100110
            // p4 01010101

            // Syndrome matrix = columns of "cover bits" above
            /*
            uint8_t H[16] = { 0u };

            const uint8_t i0 = pack_nibble(1, 0, 0, 0),
                          i1 = pack_nibble(0, 1, 1, 1),
                          i2 = pack_nibble(1, 1, 1, 0),
                          i3 = pack_nibble(1, 1, 0, 1),
                          i4 = pack_nibble(0, 1, 0, 0),
                          i5 = pack_nibble(1, 0, 1, 1),
                          i6 = pack_nibble(0, 0, 1, 0),
                          i7 = pack_nibble(0, 0, 0, 1);
            H[i0] = 0;
            H[i1] = 1;
            H[i2] = 2;
            H[i3] = 3;
            H[i4] = 4;
            H[i5] = 5;
            H[i6] = 6;
            H[i7] = 7;
            */

            static const uint8_t H[16] = { 0x0, 0x0, 0x4, 0x0, 0x6, 0x0, 0x0, 0x2,
                                           0x7, 0x0, 0x0, 0x3, 0x0, 0x5, 0x1, 0x0 };

            // Decode
            // Bit positions for data bits in codeword
            const uint8_t p1  = bit(v, 0),
                          p2  = bit(v, 4),
                          p3  = bit(v, 6),
                          p4  = bit(v, 7),
                          p1c = bit(v, 2) ^ bit(v, 3) ^ bit(v, 5),
                          p2c = bit(v, 1) ^ bit(v, 2) ^ bit(v, 3),
                          p3c = bit(v, 1) ^ bit(v, 2) ^ bit(v, 5),
                          p4c = bit(v, 1) ^ bit(v, 3) ^ bit(v, 5);

            const uint8_t syndrome = pack_nibble((uint8_t)(p1 != p1c), (uint8_t)(p2 != p2c), (uint8_t)(p3 != p3c), (uint8_t)(p4 != p4c));

            if (syndrome) {
                v ^= 1u << H[syndrome];
            }

            return pack_nibble( bit(v, 1), bit(v, 2), bit(v, 3), bit(v, 5));
        }

        template <typename T>
        inline void print_vector(std::ostream& out, const T* v, const std::string& prefix, const int size, const int element_len_bits) {
            out << prefix << ": ";

            for (int i = 0; i < size; i++)
                out << to_bin(v[i], element_len_bits) << ", ";

            out << std::endl << std::flush;
        }

        template <typename T>
        inline void print_vector_hex(std::ostream& out, const T* v, const uint32_t size, bool endline) {
            for (uint32_t i = 0u; i < size; i++) {
                out << " " << std::hex << std::setw(2) << std::setfill('0') << (int)v[i];
            }

            if(endline)
                out << std::endl;

            out << std::flush;
        }

        template <typename T>
        inline void print_interleave_matrix(std::ostream& out, const std::vector<T>& v, const uint32_t sf) {
            uint32_t cr = v.size();

            for(uint32_t i = 0; i < cr; i++)
                out << "-";
            out << std::endl;

            out << "LSB" << std::endl;

            for(int32_t i = sf-1; i >= 0; i--) {
                for(int32_t j = 0; j < (int32_t)cr; j++) {
                    out << term_colors[wrap_index(j-i, (int32_t)sf)+1] << to_bin(v[j], sf)[i] << term_colors[0];
                }
                out << std::endl;
            }

            out << "MSB" << std::endl;

            for(uint32_t i = 0; i < cr; i++)
                out << "-";
            out << std::endl;

            out << std::flush;
        }

        inline bool header_checksum(const uint8_t* header) {
            (void) header;
            /*Found valid order for bit #0: (1, 4, 8, 9, 10, 11)
            Found valid order for bit #1: (0, 2, 5, 8, 9, 10)
            Found valid order for bit #2: (0, 3, 6, 9, 11)
            Found valid order for bit #3: (1, 2, 3, 7, 8)
            Found valid order for bit #4: (4, 5, 6, 7)*/
            return true;
        }

        inline uint32_t dissect_packet(const void **header, uint32_t header_size, const uint8_t *buffer, uint32_t offset) {
            (*header) = buffer + offset;
            return offset + header_size;
        }

        inline uint32_t build_packet(uint8_t *buffer, uint32_t offset, const void* header, uint32_t header_size) {
            memcpy(buffer + offset, header, header_size);
            offset += header_size;

            return offset;
        }

    }
}

#endif /* UTILITIES_H */
