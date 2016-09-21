#ifndef UTILITIES_H
#define UTILITIES_H

namespace gr {
  namespace lora {

        template <typename T>
        std::string to_bin(T v, int element_len_bits) {
            T mask = 0;
            unsigned int maxpow = element_len_bits;
            std::string result = "";

            for(int i = 0; i < maxpow; i++) {
                mask = pow(2, i);
                //std::cout << (unsigned int)v << " AND " << mask << " is " << (v & mask) << std::endl;

                if((v & mask) > 0) {
                    result += "1";
                } else {
                    result += "0";
                }
            }

          return result;
        }

        template <typename T>
        inline void print_vector(std::vector<T>& v, std::string prefix, int element_len_bits) {
            std::cout << prefix << ": ";
            for(int i = 0; i < v.size(); i++) {
                std::cout << to_bin(v[i], element_len_bits) << ", ";
            }
            std::cout << std::endl << std::flush;
        }

        bool check_parity(std::string word, bool even) {
            int count = 0;

            for(int i = 0; i < 7; i++) {
                if(word[i] == '1')
                    count += 1;
            }

            if(even)
                return ((count % 2) == 0);
            else
                return (((count+1) % 2) == 0);
        }

        void fec_extract_data_only(uint8_t* in_data, uint32_t len, uint8_t* indices, uint8_t n, uint8_t* out_data) {
            for(uint32_t i = 0; i < len; i++) {
                uint8_t d = 0;
                for(uint32_t j = 0; j < n; j++) {
                    uint8_t power = pow(2, indices[j]);
                    if((in_data[i] & power) > 0) {
                        d += power;
                    }
                }
                out_data[i] = d;
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

        uint8_t bit(uint8_t v, uint8_t i) {
            return ((v >> i) & 0x01);
        }

        uint8_t pack_byte(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h) {
            return a | (b << 1) | (c << 2) | (d << 3) | (e << 4) | (f << 5) | (g << 6) | (h << 7);
        }

        uint8_t pack_nibble(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
            return a | (b << 1) | (c << 2) | (d << 3);
        }

        uint8_t hamming_encode_soft(uint8_t v) {
            uint8_t p1 = bit(v, 1) ^ bit(v, 2) ^ bit(v, 3);
            uint8_t p2 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 2);
            uint8_t p3 = bit(v, 0) ^ bit(v, 1) ^ bit(v, 3);
            uint8_t p4 = bit(v, 0) ^ bit(v, 2) ^ bit(v, 3);

            return pack_byte(p1, bit(v, 0), bit(v, 1), bit(v, 2), p2, bit(v, 3), p3, p4);
        }

        uint8_t hamming_decode_soft(uint8_t v) {
            // Precalculation
            // Which bits are covered (including self)?
            // p1 10110100
            // p2 01111000
            // p3 01100110
            // p4 01010101

            // Syndrome matrix = columns of "cover bits" above
            uint8_t H[16];

            for(uint8_t i = 0; i < 16; i++) {
                H[i] = 0;
            }

            uint8_t i0 = pack_nibble(1, 0, 0, 0);
            uint8_t i1 = pack_nibble(0, 1, 1, 1);
            uint8_t i2 = pack_nibble(1, 1, 1, 0);
            uint8_t i3 = pack_nibble(1, 1, 0, 1);
            uint8_t i4 = pack_nibble(0, 1, 0, 0);
            uint8_t i5 = pack_nibble(1, 0, 1, 1);
            uint8_t i6 = pack_nibble(0, 0, 1, 0);
            uint8_t i7 = pack_nibble(0, 0, 0, 1);

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
            uint8_t p1 = bit(v, 0);
            uint8_t p2 = bit(v, 4);
            uint8_t p3 = bit(v, 6);
            uint8_t p4 = bit(v, 7);
            uint8_t p1c = bit(v, 2) ^ bit(v, 3) ^ bit(v, 5);
            uint8_t p2c = bit(v, 1) ^ bit(v, 2) ^ bit(v, 3);
            uint8_t p3c = bit(v, 1) ^ bit(v, 2) ^ bit(v, 5);
            uint8_t p4c = bit(v, 1) ^ bit(v, 3) ^ bit(v, 5);

            uint8_t syndrome = pack_nibble((uint8_t)(p1 != p1c), (uint8_t)(p2 != p2c), (uint8_t)(p3 != p3c), (uint8_t)(p4 != p4c));

            if(syndrome != 0) {
                uint8_t index = H[syndrome];
                v = v ^ pow2[index];
            }

            uint8_t d1 = bit(v, 1);
            uint8_t d2 = bit(v, 2);
            uint8_t d3 = bit(v, 3);
            uint8_t d4 = bit(v, 5);

            return pack_nibble(d1, d2, d3, d4);
        }

  }
}

#endif /* UTILITIES_H */
