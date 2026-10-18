# SPDX-License-Identifier: MIT
"""Sampler primary EEM logical pairs; no FPGA package pin numbers."""
def sampler_mapping():
    return dict(zip(('adc_clk','adc_miso','sdr','cnv','pgia_clk','pgia_mosi',
                     'pgia_miso','pgia_latch'),range(8)))
