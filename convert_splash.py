import os
from PIL import Image

def convert_to_rgb565(img_path, header_path, variable_name):
    try:
        img = Image.open(img_path).convert('RGB')
    except Exception as e:
        print(f"Error opening image {img_path}: {e}")
        return

    # Target screen resolution
    img = img.resize((320, 240), Image.Resampling.LANCZOS)
    w, h = img.size
    print(f"Converting {img_path} to {w}x{h} RGB565 array...")

    with open(header_path, 'w', encoding='utf-8') as f:
        f.write("#pragma once\n\n")
        f.write(f"// Generated from {os.path.basename(img_path)}\n")
        f.write(f"const int {variable_name}_width = {w};\n")
        f.write(f"const int {variable_name}_height = {h};\n")
        f.write(f"const uint16_t {variable_name}[] = {{\n")

        pixels = list(img.getdata())
        for i, (r, g, b) in enumerate(pixels):
            rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            f.write(f"0x{rgb:04X},")
            if (i + 1) % 12 == 0:
                f.write("\n")
            else:
                f.write(" ")
                
        f.write("\n};\n")
        
    print(f"Successfully generated {header_path}")

if __name__ == "__main__":
    convert_to_rgb565('logo/mesh_touch_splash.png', 'examples/companion_radio/ui-new/mesh_touch_splash.h', 'mesh_touch_splash')
