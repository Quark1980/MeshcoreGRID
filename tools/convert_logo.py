import os
import sys
from PIL import Image

def convert_to_rgb565(img_path, header_path, variable_name):
    try:
        img = Image.open(img_path).convert('RGB')
    except Exception as e:
        print(f"Error opening image {img_path}: {e}")
        return

    # resize if too big (max 180x180) while preserving aspect ratio
    max_size = 180
    if img.width > max_size or img.height > max_size:
        img.thumbnail((max_size, max_size), Image.Resampling.LANCZOS)

    w, h = img.size
    print(f"Converting {img_path} to {w}x{h} RGB565 array...")

    with open(header_path, 'w') as f:
        f.write("#pragma once\n\n")
        f.write(f"// Generated from {os.path.basename(img_path)}\n")
        f.write(f"const int {variable_name}_width = {w};\n")
        f.write(f"const int {variable_name}_height = {h};\n")
        f.write(f"const uint16_t {variable_name}[] = {{\n")

        # Convert to RGB565 and write out
        pixels = list(img.getdata())
        for i, (r, g, b) in enumerate(pixels):
            # RGB565: 5 bits R, 6 bits G, 5 bits B
            rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            # Write as hex
            f.write(f"0x{rgb:04X}, ")
            if (i + 1) % 12 == 0:
                f.write("\n")
                
        f.write("\n};\n")
        
    print(f"Successfully generated {header_path}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python convert_logo.py <input.png> <output.h> <variable_name>")
        sys.exit(1)
    convert_to_rgb565(sys.argv[1], sys.argv[2], sys.argv[3])
