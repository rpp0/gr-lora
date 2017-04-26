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
        inline T clamp(const T d, const T min, const T max) {
            const T t = d < min ? min : d;
            return t > max ? max : t;
        }

        template <typename T>
        std::string to_bin(const T v, const uint32_t bitwidth) {
            const uint64_t maxpow = bitwidth ? (1ull << (bitwidth - 1)) : 0;
            uint64_t mask;

            std::string result = "";

            for (mask = 0x1; mask <= maxpow; mask <<= 1) {
                result += (v & mask) ? "1" : "0";
            }

            return result;
        }

        template <typename T>
        inline void print_vector(std::ostream& out, const std::vector<T>& v, const std::string& prefix, const int element_len_bits) {
            out << prefix << ": ";

            for (T x : v)
                out << to_bin(x, element_len_bits) << ", ";

            out << std::endl << std::flush;
        }

        template <typename T>
        inline void print_vector_raw(std::ostream& out, const std::vector<T>& v, const int element_len_bits) {

            for (T x : v)
                out << to_bin(x, element_len_bits);

            out << std::flush;
        }

        bool check_parity(const std::string& word, const bool even) {
            size_t count = 0, i = 0;

            while(i < 7) {
                if (word[i++] == '1')
                    ++count;
            }

            return (count & 0x1) == !even;
        }

        uint32_t select_bits(const uint32_t data, const uint8_t *indices, const uint8_t n) {
            uint32_t r = 0u;

            for(uint8_t i = 0u; i < n; ++i)
                r |= (data & (1u << indices[i])) ? (1u << i) : 0u;

            return r;
        }

        void fec_extract_data_only(const uint8_t *in_data, const uint32_t len, const uint8_t *indices, const uint8_t n, uint8_t *out_data) {
            for (uint32_t i = 0u, out_index = 0u; i < len; i += 2u) {
                uint8_t d1  = (select_bits(in_data[i], indices, n) & 0xff) << 4u;

                d1 |= (i + 1u < len) ? select_bits(in_data[i + 1u], indices, n) & 0xff : 0u;

                out_data[out_index++] = d1;
            }
        }

        inline uint8_t bit(const uint8_t v, const uint8_t i) {
            return ((v >> i) & 0x01);
        }

        inline uint8_t pack_byte(const uint8_t a, const uint8_t b, const uint8_t c, const uint8_t d,
                                 const uint8_t e, const uint8_t f, const uint8_t g, const uint8_t h) {
            return a | (b << 1) | (c << 2) | (d << 3) | (e << 4) | (f << 5) | (g << 6) | (h << 7);
        }

        inline uint8_t pack_nibble(const uint8_t a, const uint8_t b, const uint8_t c, const uint8_t d) {
            return a | (b << 1) | (c << 2) | (d << 3);
        }

        inline uint8_t hamming_encode_soft(const uint8_t v) {
            const uint8_t p1 = bit(v, 1) ^ bit(v, 2) ^ bit(v, 3);
            const uint8_t p2 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 2);
            const uint8_t p3 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 3);
            const uint8_t p4 = bit(v, 0) ^ bit(v, 2) ^ bit(v, 3);

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
                //v ^= pow2[  H[syndrome]  ];
                v ^= 1u << H[syndrome];
            }

            return pack_nibble( bit(v, 1), bit(v, 2), bit(v, 3), bit(v, 5));
        }

        // Manual Hamming
        void hamming_decode_soft(const uint8_t *words, const uint32_t len, uint8_t *out_data) {
            for (uint32_t i = 0u, out_index = 0u; i < len; i += 2u) {
                uint8_t d1 =  hamming_decode_soft_byte(words[i]) << 4u;

                d1 |= (i + 1u < len) ? hamming_decode_soft_byte(words[i + 1u]) : 0u;

                out_data[out_index++] = d1;
            }
        }

    }
}

#endif /* UTILITIES_H */
