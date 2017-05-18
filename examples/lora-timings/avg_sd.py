#!/usr/bin/python
import numpy as np


files = [
    "lora-time_SF7_grad_idx_DECODE_HEADER",         # Run 6 times (9856 samples)
    "lora-time_SF7_grad_idx_DECODE_PAYLOAD",
    "lora-time_SF7_grad_idx_DETECT",
    "lora-time_SF7_grad_idx_PAUSE",
    "lora-time_SF7_grad_idx_SYNC",
    "lora-time_SF7_grad_idx_only",                  # Run 3 times

    "lora-time_SF12_grad_idx_DECODE_HEADER",
    "lora-time_SF12_grad_idx_DECODE_PAYLOAD",
    "lora-time_SF12_grad_idx_DETECT",
    "lora-time_SF12_grad_idx_PAUSE",
    "lora-time_SF12_grad_idx_SYNC",
    "lora-time_SF12_grad_idx_only",

    "lora-time_SF7_fft_idx_DECODE_HEADER",
    "lora-time_SF7_fft_idx_DECODE_PAYLOAD",
    "lora-time_SF7_fft_idx_DETECT",
    "lora-time_SF7_fft_idx_PAUSE",
    "lora-time_SF7_fft_idx_SYNC",
    "lora-time_SF7_fft_idx_only",

    "lora-time_SF12_fft_idx_DECODE_HEADER",
    "lora-time_SF12_fft_idx_DECODE_PAYLOAD",
    "lora-time_SF12_fft_idx_DETECT",
    "lora-time_SF12_fft_idx_PAUSE",
    "lora-time_SF12_fft_idx_SYNC",
    "lora-time_SF12_fft_idx_only"
]

for name in files:
    with open("./" + name) as f:
        data = f.read()

    data = [float(x) for x in data.split('\n')[0:-1]]

    avg = np.mean(data)
    std = np.std(data, dtype=np.float64)

    print("File: {0:s}\n\tLength:  {1:d}\n\tAverage: {2:2.10f} ms\n\tStd:     {3:2.10f}"
            .format(name, len(data), avg, std))


#################
#### Results ####
#################
# File: lora-time_SF7_grad_idx_DECODE_HEADER
# 	Length:  9856
# 	Average: 0.1259104846 ms
# 	Std:     0.0083598489
# File: lora-time_SF7_grad_idx_DECODE_PAYLOAD
# 	Length:  14854
# 	Average: 0.1240467029 ms
# 	Std:     0.0058878571
# File: lora-time_SF7_grad_idx_DETECT
# 	Length:  132937
# 	Average: 0.0111668757 ms
# 	Std:     0.0863605383
# File: lora-time_SF7_grad_idx_PAUSE
# 	Length:  1232
# 	Average: 0.0010141461 ms
# 	Std:     0.0011882927
# File: lora-time_SF7_grad_idx_SYNC
# 	Length:  12334
# 	Average: 0.1369446399 ms
# 	Std:     0.0072021918
# File: lora-time_SF7_grad_idx_only
# 	Length:  11118
# 	Average: 0.1189276336 ms
# 	Std:     0.0055137726


# File: lora-time_SF12_grad_idx_DECODE_HEADER
# 	Length:  9912
# 	Average: 3.7912548676 ms
# 	Std:     0.2630857073
# File: lora-time_SF12_grad_idx_DECODE_PAYLOAD
# 	Length:  12320
# 	Average: 3.8091647075 ms
# 	Std:     0.2747977905
# File: lora-time_SF12_grad_idx_DETECT
# 	Length:  3723
# 	Average: 16.7026039911 ms
# 	Std:     21.1052133394
# File: lora-time_SF12_grad_idx_PAUSE
# 	Length:  1239
# 	Average: 0.0012905004 ms
# 	Std:     0.0002020981
# File: lora-time_SF12_grad_idx_SYNC
# 	Length:  10725
# 	Average: 4.2126484056 ms
# 	Std:     0.2068078822
# File: lora-time_SF12_grad_idx_only
# 	Length:  10002
# 	Average: 3.7576346853 ms
# 	Std:     0.2498984209


# File: lora-time_SF7_fft_idx_DECODE_HEADER
# 	Length:  9856
# 	Average: 0.0783100350 ms
# 	Std:     0.0131321480
# File: lora-time_SF7_fft_idx_DECODE_PAYLOAD
# 	Length:  20440
# 	Average: 0.0760118956 ms
# 	Std:     0.0125111634
# File: lora-time_SF7_fft_idx_DETECT
# 	Length:  130172
# 	Average: 0.0112985811 ms
# 	Std:     0.0876915475
# File: lora-time_SF7_fft_idx_PAUSE
# 	Length:  1232
# 	Average: 0.0010230820 ms
# 	Std:     0.0011295359
# File: lora-time_SF7_fft_idx_SYNC
# 	Length:  12320
# 	Average: 0.1378892978 ms
# 	Std:     0.0080917100
# File: lora-time_SF7_fft_idx_only
# 	Length:  13512
# 	Average: 0.0706166163 ms
# 	Std:     0.0098948093


# File: lora-time_SF12_fft_idx_DECODE_HEADER
# 	Length:  9912
# 	Average: 2.2331703327 ms
# 	Std:     0.2301535307
# File: lora-time_SF12_fft_idx_DECODE_PAYLOAD
# 	Length:  12390
# 	Average: 2.2217885565 ms
# 	Std:     0.2009724076
# File: lora-time_SF12_fft_idx_DETECT
# 	Length:  3744
# 	Average: 16.3153207676 ms
# 	Std:     20.9261960413
# File: lora-time_SF12_fft_idx_PAUSE
# 	Length:  1239
# 	Average: 0.0012535383 ms
# 	Std:     0.0001955853
# File: lora-time_SF12_fft_idx_SYNC
# 	Length:  10613
# 	Average: 4.2222761290 ms
# 	Std:     0.2295213521
# File: lora-time_SF12_fft_idx_only
# 	Length:  10032
# 	Average: 2.2098768243 ms
# 	Std:     0.2333799337
