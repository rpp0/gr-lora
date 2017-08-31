#include <iostream>
#include <lora/debugger.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

namespace gr {
    namespace lora {
        debugger::debugger() : d_attached(false) {
        }

        debugger::~debugger() {

        }

        /*
         * Attach to a UNIX domain socket.
         */
        void debugger::attach(std::string path) {
            if((d_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
                std::cerr << "Failed to create UNIX domain socket." << std::endl;
                return;
            }

            struct sockaddr_un address;
            memset(&address, 0, sizeof(address));
            address.sun_family = AF_UNIX;
            strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path)-1);

            if(connect(d_socket, (struct sockaddr*)&address, sizeof(address)) == -1) {
                //std::cerr << "Failed to connect to analyzer." << std::endl;
            } else {
                d_attached = true;
            }
        }

        void debugger::detach(void) {
            if(d_attached) {
                close(d_socket);
                d_attached = false;
            }
        }

        /*
        TODO: For some vague reason, making a template member function breaks swig. This only happens if we separate declaration and definition of the function (i.e. in utilities.h, the functions don't have this issue). Therefore, it seems like a bug. To resolve, we can port LoRa Receiver to C++, removing the SWIG dependency.

        See: https://github.com/gnuradio/gnuradio/search?utf8=%E2%9C%93&q=quicksort_index.h&type=
        */
        void debugger::analyze_samples(bool clear, bool draw_over) {
            if(d_attached) {
                uint32_t num_payload_bytes = d_samples.size() * sizeof(gr_complex);
                int32_t num_bytes_sent;

                debugger::header hdr;
                hdr.length = htonl(num_payload_bytes);
                hdr.draw_over = draw_over;

                // Send header
                if((num_bytes_sent = send(d_socket, &hdr, sizeof(hdr), 0)) == -1) {
                    std::cerr << "Failed to send header." << std::endl;
                    return;
                }

                // Send payload
                if((num_bytes_sent = send(d_socket, &d_samples[0], num_payload_bytes, 0)) == -1) {
                    std::cerr << "Failed to send payload." << std::endl;
                    return;
                }

                if(clear)
                    d_samples.clear();
            }
        }

        void debugger::store_samples(const gr_complex* samples, uint32_t length) {
            if(d_attached) {
                d_samples.insert(d_samples.end(), samples, samples + length);
            }
        }
    }
}
