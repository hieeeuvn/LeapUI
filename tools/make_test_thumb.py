#!/usr/bin/env python3
"""make_test_thumb.py <out.rgb565> [w] [h]

Create a fake .res box-art image (gradient + border + diagonal stripes) to exercise thumb_load.
Avoids pure-black 0x0000 pixels so it is not treated as transparent on the banner.
Defaults to 220x120 (one of the sizes thumb_load accepts).
"""
import struct, sys

def main():
    out = sys.argv[1]
    w = int(sys.argv[2]) if len(sys.argv) > 2 else 220
    h = int(sys.argv[3]) if len(sys.argv) > 3 else 120

    buf = bytearray()
    for y in range(h):
        for x in range(w):
            # background: gradient from red (left) to blue (right)
            t = x / max(w - 1, 1)
            r = int(235 - 175 * t)
            g = int(60 + 90 * t)
            b = int(60 + 150 * t)
            # 2px white border
            if x < 2 or y < 2 or x >= w - 2 or y >= h - 2:
                r, g, b = 245, 245, 245
            # 2 duong cheo (xanh la den-vua / trang) de de nhan
            elif (y - x // 2) % 26 < 5:
                r, g, b = 30, 200, 90
            elif (y + x // 3) % 34 < 4:
                r, g, b = 250, 245, 200
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            buf += struct.pack("<H", v)
    open(out, "wb").write(bytes(buf))
    print("thumb OK: %s (%dx%d, %d bytes)" % (out, w, h, len(buf)))

if __name__ == "__main__":
    main()
