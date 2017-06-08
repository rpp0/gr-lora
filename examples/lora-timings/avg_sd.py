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
