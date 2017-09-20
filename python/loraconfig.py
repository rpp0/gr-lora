class LoRaConfig():
    def __init__(self, freq, sf, cr, bw=125e3, prlen=8, crc=True, implicit=False):
        self.freq = freq
        self.sf = sf
        self.cr = cr
        self.cr_num = int(self.cr.rpartition('/')[2])-4
        self.bw = bw
        self.prlen = prlen
        self.crc = crc
        self.implicit = implicit

    def file_repr(self):
        format_string = "{:n}-sf{:n}-cr{:n}-bw{:n}".format(self.freq/1000000.0, self.sf, self.cr_num, self.bw/1000.0)
        if self.crc:
            format_string += "-crc"
        if self.implicit:
            format_string += "-imp"
        return format_string

    def string_repr(self):
        format_string = "{:n} MHz, SF {:n}, CR {:s}, BW {:n} kHz, prlen {:n}, crc {:s}, implicit {:s}".format(
            self.freq/1000000.0,
            self.sf,
            self.cr,
            self.bw/1000.0,
            self.prlen,
            "on" if self.crc else "off",
            "on" if self.implicit else "off"
        )
        return format_string
