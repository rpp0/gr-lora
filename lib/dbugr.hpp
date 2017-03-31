#ifndef DBUGR_HPP
#define DBUGR_HPP

#include <stdio.h>
#include <string.h>

#include "utilities.h"

#define CLAMP_VAL   0.7f  //1000000.0f  //0.7f

#undef  NDEBUG
//#define NDEBUG

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

#endif // DBUGR_HPP
