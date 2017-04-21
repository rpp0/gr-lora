#ifndef UTILITIES_H
#define UTILITIES_H

#include <cstdint>

namespace gr {
    namespace lora {

        /**
         *  \brief  Clamp gevin value in the given range.
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
        inline T clamp(T d, T min, T max) {
            const T t = d < min ? min : d;
            return t > max ? max : t;
        }

        template <typename T>
        std::string to_bin(T v, uint32_t bitwidth) {
            unsigned long long maxpow = bitwidth ? (1ull << (bitwidth - 1)) : 0,
                               mask;

            std::string result = "";

            for (mask = 0x1; mask <= maxpow; mask <<= 1) {
                result += (v & mask) ? "1" : "0";
            }

            return result;
        }

        template <typename T>
        inline void print_vector(std::ostream& out, std::vector<T>& v, std::string prefix, int element_len_bits) {
            out << prefix << ": ";

            for (T x : v)
                out << to_bin(x, element_len_bits) << ", ";

            out << std::endl << std::flush;
        }

        template <typename T>
        inline void print_vector_raw(std::ostream& out, std::vector<T>& v, int element_len_bits) {

            for (T x : v)
                out << to_bin(x, element_len_bits);

            out << std::flush;
        }

        bool check_parity(std::string& word, bool even) {
            size_t count = 0, i = 0;

            while(i < 7) {
                if (word[i++] == '1')
                    ++count;
            }

            return (count & 0x1) == (even ? 0 : 1);
        }

        uint32_t select_bits(uint32_t data, uint8_t *indices, uint8_t n) {
            uint32_t r = 0;

            for(uint8_t i = 0; i < n; ++i)
                r |= (data & (1 << indices[i])) ? (1 << i) : 0;

            return r;
        }

        void fec_extract_data_only(uint8_t *in_data, uint32_t len, uint8_t *indices, uint8_t n, uint8_t *out_data) {
            for (uint32_t i = 0, out_index = 0; i < len; i += 2) {
                uint8_t d1  = (select_bits(in_data[i], indices, n) & 0xff) << 4;

                d1 |= (i + 1 < len) ? select_bits(in_data[i + 1], indices, n) & 0xff : 0;

                out_data[out_index++] = d1;
            }
        }


        uint8_t pow2[8] = {
            0x01,
            0x02,
            0x04,
            0x08,
            0x10,
            0x20,
            0x40,
            0x80
        };

        inline uint8_t bit(uint8_t v, uint8_t i) {
            return ((v >> i) & 0x01);
        }

        inline uint8_t pack_byte(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h) {
            return a | (b << 1) | (c << 2) | (d << 3) | (e << 4) | (f << 5) | (g << 6) | (h << 7);
        }

        inline uint8_t pack_nibble(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
            return a | (b << 1) | (c << 2) | (d << 3);
        }

        inline uint8_t hamming_encode_soft(uint8_t v) {
            uint8_t p1 = bit(v, 1) ^ bit(v, 2) ^ bit(v, 3);
            uint8_t p2 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 2);
            uint8_t p3 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 3);
            uint8_t p4 = bit(v, 0) ^ bit(v, 2) ^ bit(v, 3);

            return pack_byte(p1, bit(v, 0), bit(v, 1), bit(v, 2), p2, bit(v, 3), p3, p4);
        }

        uint8_t hamming_decode_soft_byte(uint8_t v) {
            // Precalculation
            // Which bits are covered (including self)?
            // p1 10110100
            // p2 01111000
            // p3 01100110
            // p4 01010101

            // Syndrome matrix = columns of "cover bits" above
            uint8_t H[16] = { 0 };

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
                v ^= pow2[  H[syndrome]  ];
            }

            return pack_nibble( bit(v, 1), bit(v, 2), bit(v, 3), bit(v, 5));
        }

        // Manual Hamming
        void hamming_decode_soft(uint8_t *words, uint32_t len, uint8_t *out_data) {
            for (uint32_t i = 0, out_index = 0; i < len; i += 2) {
                uint8_t d1 =  hamming_decode_soft_byte(words[i]) << 4;

                d1 |= (i + 1 < len) ? hamming_decode_soft_byte(words[i + 1]) : 0;

                out_data[out_index++] = d1;
            }
        }

    }
}

#endif /* UTILITIES_H */
