"""Draws the SwiftPlace application icon and writes swiftplace.ico + a 256 px PNG.

Standard library only, so it runs with any Python 3:  python make_icon.py

The picture, on a 256 x 256 grid: a blue rounded tile, a placement nozzle
coming down onto a chip (SOIC body with leads), a green board strip below it,
and motion lines beside the nozzle. Motion lines are dropped below 32 px, where
they would only blur.
"""

import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
SUPERSAMPLE = 4


def hex_color(text, alpha=1.0):
    text = text.lstrip("#")
    return (int(text[0:2], 16) / 255, int(text[2:4], 16) / 255, int(text[4:6], 16) / 255, alpha)


def lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


class RoundRect:
    def __init__(self, x0, y0, x1, y1, r, fill):
        self.x0, self.y0, self.x1, self.y1, self.r, self.fill = x0, y0, x1, y1, r, fill
        self.box = (x0, y0, x1, y1)

    def contains(self, x, y):
        if x < self.x0 or x > self.x1 or y < self.y0 or y > self.y1:
            return False
        cx = min(max(x, self.x0 + self.r), self.x1 - self.r)
        cy = min(max(y, self.y0 + self.r), self.y1 - self.r)
        return (x - cx) ** 2 + (y - cy) ** 2 <= self.r ** 2

    def color(self, x, y):
        return self.fill(x, y) if callable(self.fill) else self.fill


class Polygon:
    """Convex polygon, points listed clockwise on screen (y down)."""

    def __init__(self, points, fill):
        self.points, self.fill = points, fill
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        self.box = (min(xs), min(ys), max(xs), max(ys))

    def contains(self, x, y):
        n = len(self.points)
        for i in range(n):
            ax, ay = self.points[i]
            bx, by = self.points[(i + 1) % n]
            if (bx - ax) * (y - ay) - (by - ay) * (x - ax) < 0:
                return False
        return True

    def color(self, x, y):
        return self.fill


class Capsule:
    """A line segment with round ends."""

    def __init__(self, ax, ay, bx, by, r, fill):
        self.ax, self.ay, self.bx, self.by, self.r, self.fill = ax, ay, bx, by, r, fill
        self.box = (min(ax, bx) - r, min(ay, by) - r, max(ax, bx) + r, max(ay, by) + r)

    def contains(self, x, y):
        dx, dy = self.bx - self.ax, self.by - self.ay
        t = ((x - self.ax) * dx + (y - self.ay) * dy) / (dx * dx + dy * dy)
        t = min(max(t, 0.0), 1.0)
        px, py = self.ax + dx * t, self.ay + dy * t
        return (x - px) ** 2 + (y - py) ** 2 <= self.r ** 2

    def color(self, x, y):
        return self.fill


class Circle:
    def __init__(self, cx, cy, r, fill):
        self.cx, self.cy, self.r, self.fill = cx, cy, r, fill
        self.box = (cx - r, cy - r, cx + r, cy + r)

    def contains(self, x, y):
        return (x - self.cx) ** 2 + (y - self.cy) ** 2 <= self.r ** 2

    def color(self, x, y):
        return self.fill


TILE_TOP = hex_color("#3B8BFF")
TILE_BOTTOM = hex_color("#1546B0")
WHITE = hex_color("#FFFFFF")
STEEL = hex_color("#E9EDF3")
STEEL_DARK = hex_color("#B8C2D0")
CHIP = hex_color("#1C2230")
CHIP_EDGE = hex_color("#2C3546")
LEAD = hex_color("#D5DBE4")
BOARD = hex_color("#3DDC97")


def tile_gradient(x, y):
    return lerp(TILE_TOP, TILE_BOTTOM, min(max((y - 8) / 240, 0.0), 1.0))


def shapes(size):
    result = [RoundRect(8, 8, 248, 248, 52, tile_gradient)]

    if size >= 32:
        result += [
            Capsule(84, 40, 84, 82, 5, hex_color("#FFFFFF", 0.55)),
            Capsule(172, 48, 172, 88, 5, hex_color("#FFFFFF", 0.55)),
            Capsule(62, 62, 62, 92, 4, hex_color("#FFFFFF", 0.30)),
            Capsule(194, 70, 194, 98, 4, hex_color("#FFFFFF", 0.30)),
        ]

    # Board strip the chip is placed onto.
    result.append(RoundRect(36, 208, 220, 226, 8, BOARD))

    # Leads, four per side, under the body.
    for cy in (134, 152.5, 171, 189.5):
        result.append(RoundRect(46, cy - 5.5, 72, cy + 5.5, 2.5, LEAD))
        result.append(RoundRect(184, cy - 5.5, 210, cy + 5.5, 2.5, LEAD))

    # Chip body with a lighter rim and a pin-1 mark.
    result.append(RoundRect(62, 118, 194, 202, 12, CHIP_EDGE))
    result.append(RoundRect(68, 124, 188, 196, 8, CHIP))
    result.append(Circle(86, 142, 7, hex_color("#4A5568")))

    # Nozzle: collar, shaft, tapered tip resting on the chip.
    result.append(RoundRect(104, 22, 152, 40, 6, WHITE))
    result.append(RoundRect(116, 36, 140, 98, 3, STEEL))
    result.append(Polygon([(112, 94), (144, 94), (135, 122), (121, 122)], STEEL_DARK))
    return result


def render(size):
    scale = 256 / size
    items = shapes(size)
    step = 1 / SUPERSAMPLE
    offsets = [(i + 0.5) * step for i in range(SUPERSAMPLE)]
    samples = SUPERSAMPLE * SUPERSAMPLE
    pixels = bytearray()
    for py in range(size):
        pixels.append(0)  # PNG filter type: none
        row_items = [s for s in items if s.box[1] <= (py + 1) * scale and s.box[3] >= py * scale]
        for px in range(size):
            cell = [s for s in row_items if s.box[0] <= (px + 1) * scale and s.box[2] >= px * scale]
            acc = [0.0, 0.0, 0.0, 0.0]  # premultiplied
            for oy in offsets:
                y = (py + oy) * scale
                for ox in offsets:
                    x = (px + ox) * scale
                    r = g = b = a = 0.0
                    for s in cell:
                        if s.contains(x, y):
                            sr, sg, sb, sa = s.color(x, y)
                            r = sr * sa + r * (1 - sa)
                            g = sg * sa + g * (1 - sa)
                            b = sb * sa + b * (1 - sa)
                            a = sa + a * (1 - sa)
                    acc[0] += r
                    acc[1] += g
                    acc[2] += b
                    acc[3] += a
            a = acc[3] / samples
            if a > 0:
                rgb = [min(255, round(c / samples / a * 255)) for c in acc[:3]]
            else:
                rgb = [0, 0, 0]
            pixels += bytes(rgb + [round(a * 255)])
    return png(size, size, bytes(pixels))


def png(width, height, raw):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def ico(images):
    """images: list of (size, png bytes). PNG entries are valid since Windows Vista."""
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for size, data in images:
        dim = 0 if size >= 256 else size
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for _, data in images:
        out += data
    return out


def main():
    images = []
    for size in ICO_SIZES:
        images.append((size, render(size)))
        print("rendered", size)
    with open(os.path.join(HERE, "swiftplace.ico"), "wb") as f:
        f.write(ico(images))
    with open(os.path.join(HERE, "swiftplace-256.png"), "wb") as f:
        f.write(images[-1][1])
    print("wrote swiftplace.ico and swiftplace-256.png")


if __name__ == "__main__":
    main()
