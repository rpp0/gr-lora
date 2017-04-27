#ifndef DBUGR_HPP
#define DBUGR_HPP

#include <stdio.h>
#include <string.h>
#include <chrono>

#include "utilities.h"

#define CLAMP_VAL   0.7f  //1000000.0f  //0.7f

#undef NDEBUG            /// Debug printing
//#define NDEBUG           /// No debug printing

//#define DBGR_CHRONO      /// Measure execution time

#ifdef NDEBUG
    #define DBGR_PAUSE(MSG)
    #define DBGR_QUICK_TO_FILE(FILEPATH, APPEND, DATA, SIZE, FORMAT)
    #define DBGR_WRITE_SIGNAL(IDEAL_SIG_FP, SAMPLE_SIG_FP, WINDOW, OFFSET, MIN, MAX, FULL, PAUSE, MSG)
#else
    #define DBGR_PAUSE(MSG)     system("read -rsp $'" #MSG "\nPress any key to continue...\n' -n 1 key")

    #define DBGR_QUICK_TO_FILE(FILEPATH, APPEND, DATA, SIZE, FORMAT)                                            \
        do {                                                                                                    \
            int32_t DBGR_j;                                                                                     \
            char DBGR_buf[20];                                                                                  \
            std::ofstream DBGR_out_file;                                                                        \
            DBGR_out_file.open(FILEPATH, std::ios::out | (APPEND ? std::ios::app : std::ios::out ));            \
                                                                                                                \
            for (DBGR_j = 0; DBGR_j < int32_t(SIZE); DBGR_j++) {                                                \
                sprintf(DBGR_buf, FORMAT, DATA[DBGR_j]);                                                        \
                DBGR_out_file.write(DBGR_buf, strlen(DBGR_buf));                                                \
            }                                                                                                   \
                                                                                                                \
            if (APPEND) DBGR_out_file.write("\n", sizeof(char));                                                \
                                                                                                                \
        } while(0)                                                                                              \

    #define DBGR_WRITE_SIGNAL(IDEAL_SIG_FP, SAMPLE_SIG_FP, WINDOW, OFFSET, MIN, MAX, FULL, PAUSE, MSG)          \
        do {                                                                                                    \
            int32_t DBGR_j;                                                                                     \
            char DBGR_buf[20];                                                                                  \
            const char DBGR_delim[] = "-----------------------------------------------------------------\n\0";  \
            std::ofstream DBGR_out_file;                                                                        \
            DBGR_out_file.open("/tmp/DBGR.txt", std::ios::out);                                                 \
                                                                                                                \
                                                                                                                \
            /*printf("DBGR_Ideal\n");*/                                                                             \
            for (DBGR_j = 0; DBGR_j < int32_t(WINDOW); DBGR_j++) {                                              \
                sprintf(DBGR_buf, "%f\n", IDEAL_SIG_FP[DBGR_j]);                                                \
                DBGR_out_file.write(DBGR_buf,   strlen(DBGR_buf));                                              \
            }   DBGR_out_file.write(DBGR_delim, strlen(DBGR_delim));                                            \
                                                                                                                \
            /*printf("%s", DBGR_delim);*/                                                                           \
                                                                                                                \
            /*printf("DBGR_Before\n");*/                                                                            \
            for (DBGR_j = 0; DBGR_j < int32_t(WINDOW); DBGR_j++) {                                              \
                sprintf(DBGR_buf, "%f\n", gr::lora::clamp(SAMPLE_SIG_FP[DBGR_j], -CLAMP_VAL, CLAMP_VAL));                 \
                DBGR_out_file.write(DBGR_buf,   strlen(DBGR_buf));                                              \
            }   DBGR_out_file.write(DBGR_delim, strlen(DBGR_delim));                                            \
                                                                                                                \
            /*printf("%s", DBGR_delim);*/                                                                           \
                                                                                                                \
            printf("DBGR_After %d of %d in %d\n", MIN, MAX, WINDOW);                                            \
            for (DBGR_j = OFFSET; DBGR_j < int32_t(OFFSET > 0 ? WINDOW : MAX); DBGR_j++) {                      \
                sprintf(DBGR_buf, "%f\n", gr::lora::clamp(*(SAMPLE_SIG_FP + DBGR_j), -CLAMP_VAL, CLAMP_VAL));             \
                DBGR_out_file.write(DBGR_buf,   strlen(DBGR_buf));                                              \
            }   DBGR_out_file.write(DBGR_delim, strlen(DBGR_delim));                                            \
                                                                                                                \
            printf("%s", DBGR_delim);                                                                           \
                                                                                                                \
            /*if (FULL) {                                                                                       \
                printf("DBGR_Full\n");                                                                          \
                for (DBGR_j = 0; DBGR_j < WINDOW; DBGR_j++) {                                                   \
                    sprintf(DBGR_buf, "%f\n\0", SAMPLE_SIG_FP[DBGR_j]);                                         \
                    DBGR_out_file.write(DBGR_buf,   strlen(DBGR_buf));                                          \
                }   DBGR_out_file.write(DBGR_delim, strlen(DBGR_delim));                                        \
                printf("%s", DBGR_delim);                                                                       \
            }*/                                                                                                 \
                                                                                                                \
            /*printf("DBGR_End\n");*/                                                                               \
            DBGR_out_file.close();                                                                              \
            if(PAUSE) DBGR_PAUSE(MSG);                                                                          \
        } while(0)
#endif

// Chrono
#ifndef DBGR_CHRONO
    #define DBGR_START_TIME_MEASUREMENT(OUT, MSG)
    #define DBGR_START_TIME_MEASUREMENT_SAME_SCOPE(OUT, MSG)
    #define DBGR_INTERMEDIATE_TIME_MEASUREMENT()
    #define DBGR_STOP_TIME_MEASUREMENT(OUT)
#else
    static int64_t DBGR_global_time    = 0ll;
    static bool DBGR_totalled_time     = false;
    static bool DBGR_intermediate_time = false;

    #define DBGR_START_TIME_MEASUREMENT(OUT, MSG) \
        DBGR_intermediate_time = OUT; \
        if (DBGR_intermediate_time) printf("[CHRONO] : Start in % 15s", MSG); \
        DBGR_totalled_time = false; \
        auto DBGR_start_time = std::chrono::high_resolution_clock::now();

    #define DBGR_START_TIME_MEASUREMENT_SAME_SCOPE(OUT, MSG) \
        DBGR_intermediate_time = OUT; \
        if (DBGR_intermediate_time) printf("[CHRONO] : Start with % 15s", MSG); \
        DBGR_totalled_time = false; \
        DBGR_start_time = std::chrono::high_resolution_clock::now();

    #define DBGR_INTERMEDIATE_TIME_MEASUREMENT() \
        if (!DBGR_totalled_time) { \
            int64_t DBGR_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - DBGR_start_time).count(); \
            DBGR_global_time += DBGR_duration; \
            if (DBGR_intermediate_time) printf(" and took %fms\n", DBGR_duration / 1e6); \
        }

    #define DBGR_STOP_TIME_MEASUREMENT(OUT) \
        if (OUT) printf("[CHRONO] : Packet took %fms to process.\n", DBGR_global_time / 1e6); \
        DBGR_global_time = 0ll; \
        DBGR_totalled_time = true;
#endif

#endif // DBUGR_HPP
