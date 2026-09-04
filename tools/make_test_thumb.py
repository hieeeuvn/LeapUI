#!/usr/bin/env python3
"""make_test_thumb.py <out.rgb565> [w] [h]

Tao anh .res gia kieu boxart (gradient + vien + duong cheo) de test thumb_load.
Khong dung pixel 0x0000 (den tinh khiet) -> khong bi trong suot khi ve len banner.
Mac dinh 220x120 (1 trong cac size thumb_load nhan).
"""
import struct, sys

def main():
    out = sys.argv[1]
    w = int(sys.argv[2]) if len(sys.argv) > 2 else 220
    h = int(sys.argv[3]) if len(sys.argv) > 3 else 120

    buf = bytearray()
    for y in range(h):
        for x in range(w):
            # nen: gradient trai do -> phai xanh duong
            t = x / max(w - 1, 1)
            r = int(235 - 175 * t)
            g = int(60 + 90 * t)
            b = int(60 + 150 * t)
            # vien trang day 2
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
