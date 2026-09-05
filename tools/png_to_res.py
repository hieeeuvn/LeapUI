#!/usr/bin/env python3
"""png_to_res.py <in.png> <out.rgb565> <W> <H>

Convert a PNG (box art / cart sticker) to the raw little-endian RGB565 .res
format used by LeapUI thumb_load(). Resizes with premultiplied area-average
downsampling so scaled-down art stays clean; fully transparent pixels become
0x0000 (which LeapUI draws as transparent, giving rounded-corner art a clean
look). Note: opaque pure-black pixels also map to 0x0000 and are therefore
drawn transparent by the UI - a known quirk of the .res format.

Stdlib only (zlib for IDAT). Supports 8/16-bit gray/RGB/palette/grayA/RGBA,
non-interlaced.
"""
import struct, sys, zlib, math

def read_png(path):
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG file"
    pos, idat = 8, b""
    w = h = bd = ct = il = 0
    plte = trns = None
    while pos + 8 <= len(d):
        ln = struct.unpack_from(">I", d, pos)[0]
        tag, data = d[pos + 4:pos + 8], d[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h, bd, ct, cm, fm, il = struct.unpack(">IIBBBBB", data)
        elif tag == b"IDAT":
            idat += data
        elif tag == b"PLTE":
            plte = data
        elif tag == b"tRNS":
            trns = data
        pos += 12 + ln
    assert il == 0, "interlaced PNG not supported"
    assert cm == 0, "unsupported compression"
    assert fm == 0, "unsupported filter method"
    return w, h, bd, ct, il, plte, trns, zlib.decompress(idat)

def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)

def decode(w, h, bd, ct, plte, trns, raw):
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    sbpp = ch * (1 if bd == 8 else 2)
    stride = (w * bd * ch + 7) // 8
    px = bytearray(w * h * 4)          # RGBA output
    prev = bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if f == 1:
            for i in range(sbpp, stride): line[i] = (line[i] + line[i - sbpp]) & 0xFF
        elif f == 2:
            for i in range(stride): line[i] = (line[i] + prev[i]) & 0xFF
        elif f == 3:
            for i in range(stride):
                a = line[i - sbpp] if i >= sbpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif f == 4:
            for i in range(stride):
                a = line[i - sbpp] if i >= sbpp else 0
                c = prev[i - sbpp] if i >= sbpp else 0
                line[i] = (line[i] + paeth(a, prev[i], c)) & 0xFF
        for x in range(w):
            base = x * ch * (1 if bd == 8 else 2)
            o = (y * w + x) * 4
            def gv(i):
                if bd == 8:
                    return line[base + i]
                return (line[base + i * 2] << 8) | line[base + i * 2 + 1]
            if ct == 6:    # RGBA
                r, g, b, a = gv(0), gv(1), gv(2), gv(3)
            elif ct == 2:  # RGB
                r, g, b = gv(0), gv(1), gv(2)
                a = 0 if trns and (r, g, b) == struct.unpack(">3H", trns) else 255
            elif ct == 4:  # gray + alpha
                r = g = b = gv(0); a = gv(1)
            elif ct == 0:  # gray
                r = g = b = gv(0)
                a = 0 if trns and r == struct.unpack(">H", trns)[0] else 255
            else:          # palette
                idx = gv(0)
                r, g, b = plte[idx * 3:idx * 3 + 3]
                a = trns[idx] if trns and idx < len(trns) else 255
            px[o:o + 4] = bytes((r, g, b, a))
        prev = line
    return px

def scale_down(px, sw, sh, dw, dh):
    """Premultiplied area average (colour weighted by alpha so edges stay clean)."""
    sx, sy = sw / dw, sh / dh
    out = bytearray(dw * dh * 4)
    for oy in range(dh):
        y0, y1 = oy * sy, (oy + 1) * sy
        ys = range(max(0, int(math.floor(y0))), min(sh, int(math.ceil(y1))))
        for ox in range(dw):
            x0, x1 = ox * sx, (ox + 1) * sx
            xs = range(max(0, int(math.floor(x0))), min(sw, int(math.ceil(x1))))
            ar = ag = ab = aa = 0.0
            for sy0 in ys:
                hov = min(sy0 + 1, y1) - max(sy0, y0)
                if hov <= 0: continue
                for sx0 in xs:
                    wov = min(sx0 + 1, x1) - max(sx0, x0)
                    if wov <= 0: continue
                    i = (sy0 * sw + sx0) * 4
                    a = px[i + 3]
                    ov = hov * wov * a
                    aa += ov
                    ar += px[i] * ov; ag += px[i + 1] * ov; ab += px[i + 2] * ov
            o = (oy * dw + ox) * 4
            if aa > 0:
                out[o:o + 4] = bytes((int(ar / aa), int(ag / aa), int(ab / aa), 255))
            else:
                out[o:o + 4] = bytes((0, 0, 0, 0))
    return out

def scale_up(px, sw, sh, dw, dh):
    out = bytearray(dw * dh * 4)
    for oy in range(dh):
        y = min(sh - 1, max(0, (oy + 0.5) * sh / dh - 0.5))
        y0 = int(y)
        for ox in range(dw):
            x = min(sw - 1, max(0, (ox + 0.5) * sw / dw - 0.5))
            x0 = int(x)
            i = (y0 * sw + x0) * 4
            o = (oy * dw + ox) * 4
            out[o:o + 4] = px[i:i + 4]
    return out

def to_rgb565(px, n):
    buf = bytearray(n * 2)
    for i in range(n):
        r, g, b, a = px[i * 4:i * 4 + 4]
        if a == 0:
            v = 0x0000
        else:
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        struct.pack_into("<H", buf, i * 2, v)
    return buf

def main():
    src, dst = sys.argv[1], sys.argv[2]
    dw, dh = int(sys.argv[3]), int(sys.argv[4])
    w, h, bd, ct, il, plte, trns, raw = read_png(src)
    px = decode(w, h, bd, ct, plte, trns, raw)
    corners = [px[0:4], px[(h - 1) * w * 4:(h - 1) * w * 4 + 4], px[(w - 1) * 4:(w - 1) * 4 + 4],
               px[((h - 1) * w + w - 1) * 4:((h - 1) * w + w - 1) * 4 + 4]]
    opaque = sum(1 for i in range(0, w * h * 4, 4) if px[i + 3] > 0)
    print("PNG %dx%d depth=%d ct=%d transparent=%d%% corners=%s" %
          (w, h, bd, ct, 100 - opaque * 100 // (w * h),
           [(c[0], c[1], c[2], c[3]) for c in corners]))
    if dw != w or dh != h:
        scale = (dw < w or dh < h)
        px = scale_down(px, w, h, dw, dh) if scale else scale_up(px, w, h, dw, dh)
        print("resized -> %dx%d (%s)" % (dw, dh, "area-avg" if scale else "nearest"))
    else:
        print("no resize (already %dx%d)" % (dw, dh))
    buf = to_rgb565(px, dw * dh)
    open(dst, "wb").write(bytes(buf))
    blacks = sum(1 for i in range(0, len(buf), 2) if buf[i] == 0 and buf[i + 1] == 0)
    print("res OK: %s (%dx%d, %d bytes, %d pure-black px become transparent)" %
          (dst, dw, dh, len(buf), blacks))

if __name__ == "__main__":
    main()
