from pathlib import Path
import sys

try:
    from PIL import Image, ImageOps
except ImportError:
    print("Pillow is required for this PC-side conversion tool.")
    print("Install it with: pip install pillow")
    sys.exit(1)


ROOT = Path(__file__).resolve().parents[1]
INPUT_DIR = ROOT / "src" / "data" / "UI"
OUTPUT_FILE = INPUT_DIR / "cyberpunk_bg_240_rgb565.h"
SIZE = 240
BACKGROUND_BRIGHTNESS = 1.00
MAP_DOT_BRIGHTNESS = 1.28
KEYWORDS = ("cyberpunk", "cyber", "background", "bg")
CYAN_TEXT_REPLACEMENT = (0, 8, 18)

BEARING_NUMBER_BOXES = (
    (55, 22, 79, 36),
    (162, 22, 187, 36),
    (199, 61, 224, 76),
    (199, 168, 224, 184),
    (162, 204, 187, 220),
    (53, 204, 78, 220),
    (17, 168, 43, 184),
    (17, 61, 43, 76),
)

CARDINAL_TEXT_BOXES = (
    (111, 7, 129, 27),
    (6, 110, 25, 130),
    (111, 213, 129, 234),
    (215, 110, 234, 130),
)

MAP_DOT_BOXES = (
    (28, 43, 101, 191),
    (142, 146, 184, 180),
    (82, 151, 133, 190),
)


def image_score(path: Path) -> int:
    name = path.name.lower()
    score = 0
    for keyword in KEYWORDS:
        if keyword in name:
            score += 10
    if "240" in name:
        score += 2
    return score


def find_input_image() -> Path:
    candidates = sorted(
        [
            path
            for path in INPUT_DIR.iterdir()
            if path.is_file() and path.suffix.lower() in (".png", ".jpg", ".jpeg")
        ],
        key=lambda path: path.name.lower(),
    )

    if not candidates:
        raise FileNotFoundError(f"No png/jpg/jpeg image found in {INPUT_DIR}")

    ranked = sorted(candidates, key=lambda path: image_score(path), reverse=True)
    best_score = image_score(ranked[0])
    tied = [path for path in ranked if image_score(path) == best_score]
    if len(candidates) > 1:
        print("Found multiple images:")
        for path in candidates:
            print(f"  {path.name} score={image_score(path)}")
    if len(tied) > 1:
        print("Multiple images have the same priority; using the first sorted name.")

    return ranked[0]


def color565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def is_cyan_text_pixel(r: int, g: int, b: int) -> bool:
    return g > 70 and b > 90 and r < 95 and g > r * 1.7 and b > r * 1.7


def erase_cyan_text_in_box(image: Image.Image, box: tuple[int, int, int, int]) -> None:
    pixels = image.load()
    left, top, right, bottom = box
    for y in range(max(0, top), min(SIZE, bottom)):
        for x in range(max(0, left), min(SIZE, right)):
            r, g, b = pixels[x, y]
            if is_cyan_text_pixel(r, g, b):
                pixels[x, y] = CYAN_TEXT_REPLACEMENT


def point_in_boxes(x: int, y: int, boxes: tuple[tuple[int, int, int, int], ...]) -> bool:
    for left, top, right, bottom in boxes:
        if left <= x < right and top <= y < bottom:
            return True
    return False


def is_map_dot_pixel(r: int, g: int, b: int) -> bool:
    return b > 95 and g > 75 and r < 60 and b > r * 2.0 and g > r * 1.8


def apply_static_hud_edits(image: Image.Image) -> None:
    for box in BEARING_NUMBER_BOXES:
        erase_cyan_text_in_box(image, box)
    for box in CARDINAL_TEXT_BOXES:
        erase_cyan_text_in_box(image, box)


def apply_brightness(value: int) -> int:
    return max(0, min(255, int(round(value * BACKGROUND_BRIGHTNESS))))


def apply_factor(value: int, factor: float) -> int:
    return max(0, min(255, int(round(value * factor))))


def load_240_rgb565(input_file: Path) -> list[int]:
    with Image.open(input_file) as image:
        original_size = image.size
        image = ImageOps.fit(image.convert("RGB"), (SIZE, SIZE), method=Image.Resampling.LANCZOS)
        apply_static_hud_edits(image)
        pixels = image.load()
        values: list[int] = []
        center = (SIZE - 1) / 2.0
        radius_sq = (SIZE / 2.0) * (SIZE / 2.0)
        for y in range(SIZE):
            for x in range(SIZE):
                dx = x - center
                dy = y - center
                if dx * dx + dy * dy > radius_sq:
                    values.append(0x0000)
                    continue

                r, g, b = pixels[x, y]
                if point_in_boxes(x, y, MAP_DOT_BOXES) and is_map_dot_pixel(r, g, b):
                    r = apply_factor(r, MAP_DOT_BRIGHTNESS)
                    g = apply_factor(g, MAP_DOT_BRIGHTNESS)
                    b = apply_factor(b, MAP_DOT_BRIGHTNESS)

                r = apply_brightness(r)
                g = apply_brightness(g)
                b = apply_brightness(b)
                values.append(color565(r, g, b))

    print(f"Input:  {input_file}")
    print(f"Output: {OUTPUT_FILE}")
    print(f"Source size: {original_size[0]}x{original_size[1]}")
    print(f"Output size: {SIZE}x{SIZE}")
    return values


def write_header(values: list[int], source_file: Path) -> None:
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT_FILE.open("w", encoding="utf-8", newline="\n") as header:
        header.write("#pragma once\n")
        header.write("#include <Arduino.h>\n\n")
        header.write("// Generated by tools/convert_cyberpunk_bg.py\n")
        header.write(f"// Source: {source_file.name}\n")
        header.write(f"// Background brightness: {BACKGROUND_BRIGHTNESS:.2f}x\n\n")
        header.write("static const uint16_t CYBERPUNK_BG_WIDTH = 240;\n")
        header.write("static const uint16_t CYBERPUNK_BG_HEIGHT = 240;\n\n")
        header.write("static const uint16_t CYBERPUNK_BG_240[240 * 240] PROGMEM =\n")
        header.write("{\n")

        values_per_line = 12
        for index in range(0, len(values), values_per_line):
            line_values = values[index:index + values_per_line]
            formatted = ", ".join(f"0x{value:04X}" for value in line_values)
            comma = "," if index + values_per_line < len(values) else ""
            header.write(f"    {formatted}{comma}\n")

        header.write("};\n")


def main() -> int:
    try:
        input_file = find_input_image()
        values = load_240_rgb565(input_file)
        write_header(values, input_file)
    except Exception as exc:
        print(f"Conversion failed: {exc}")
        return 1

    print("RGB565 header generated successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
