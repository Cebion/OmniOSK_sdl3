#!/usr/bin/env python3
import sys


def read_ppm(path):
    with open(path, "rb") as image:
        if image.readline().strip() != b"P6":
            raise AssertionError("expected binary PPM")
        dimensions = image.readline().split()
        width, height = int(dimensions[0]), int(dimensions[1])
        if int(image.readline()) != 255:
            raise AssertionError("expected 8-bit PPM")
        pixels = image.read()
    if len(pixels) != width * height * 3:
        raise AssertionError("truncated PPM")
    return width, height, pixels


def is_key_pixel(pixels, width, x, y):
    red, green, blue = pixels[(y * width + x) * 3:(y * width + x + 1) * 3]
    surface = red + green + blue > 150 and red > 34 and green > 40 and blue > 45 and blue - red < 130
    focused_key = red > 50 and green > red + 22 and blue > green + 20 and blue < 200
    return surface or focused_key


def bands(values):
    result = []
    start = None
    for index, present in enumerate(values + [False]):
        if present and start is None:
            start = index
        elif not present and start is not None:
            result.append((start, index - 1))
            start = None
    return result


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit("usage: check_keyboard_image.py <gles3|renderer> IMAGE PAGE [MIN_FIRST_ROW_KEYS]")
    backend = sys.argv[1]
    page = sys.argv[3]
    minimum_keys = int(sys.argv[4]) if len(sys.argv) == 5 else 8
    width, height, pixels = read_ppm(sys.argv[2])
    first_scanline = int(height * 0.35)
    row_counts = [
        sum(is_key_pixel(pixels, width, x, y) for x in range(width))
        for y in range(first_scanline, height)
    ]
    row_bands = [(start + first_scanline, end + first_scanline) for start, end in bands([count >= 40 for count in row_counts])]
    expected_rows = 5 if page == "special" else 4
    if len(row_bands) < expected_rows:
        raise AssertionError(f"{backend}: expected {expected_rows} keyboard rows for {page}, found {row_bands}")

    key_bands = row_bands[-expected_rows:]
    first_start, first_end = key_bands[0]
    sample_row = (first_start + first_end) // 2
    runs = []
    start = None
    for x in range(width + 1):
        present = x < width and is_key_pixel(pixels, width, x, sample_row)
        if present and start is None:
            start = x
        elif not present and start is not None:
            if x - start >= 12:
                runs.append((start, x - 1))
            start = None
    if len(runs) < minimum_keys:
        raise AssertionError(f"{backend}: expected {minimum_keys} keys in the first row, found {runs}")


if __name__ == "__main__":
    main()
