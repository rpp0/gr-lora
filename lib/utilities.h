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

        
  }
}

#endif /* UTILITIES_H */
