#!/usr/bin/env python3
"""rgb565_to_png.py <input.rgb565> <output.png> [scale] [w] [h]

Convert raw little-endian RGB565 to PNG (stdlib only). Defaults to 320x240, scale 3 for readability.
"""
import struct, sys, zlib

def main():
    src, dst = sys.argv[1], sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    w = int(sys.argv[4]) if len(sys.argv) > 4 else 320
    h = int(sys.argv[5]) if len(sys.argv) > 5 else 240

    raw = open(src, "rb").read()
    expect = w * h * 2
    if len(raw) < expect:
        print("not enough data: %d < %d" % (len(raw), expect), file=sys.stderr)
        sys.exit(1)

    def px(i):
        v = struct.unpack_from("<H", raw, i * 2)[0]
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        return (r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31

    # scale (nearest)
    W, H = w * scale, h * scale
    rows = []
    for y in range(H):
        sy = y // scale
        row = bytearray([0])
        for x in range(W):
            sx = x // scale
            row += bytes(px(sy * w + sx))
        rows.append(bytes(row))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(b"".join(rows), 6)) + chunk(b"IEND", b"")
    open(dst, "wb").write(png)
    print("PNG OK: %s (%dx%d)" % (dst, W, H))

if __name__ == "__main__":
    main()
