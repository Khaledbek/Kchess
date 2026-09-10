from PIL import Image
input_datei = 'checkmat_img.png'
def farben_aendern(input_datei, output_datei):
    img = Image.open(input_datei).convert("RGBA")
    pixel = img.load()

    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = pixel[x, y]

            # Vollständig transparente Pixel nicht verändern
            if a == 0:
                continue

            # Rot -> Grün
            if r > 150 and g < 100 and b < 100:
                pixel[x, y] = (0, 200, 0, a)

            # Schwarz -> Weiß
            elif r < 80 and g < 80 and b < 80:
                pixel[x, y] = (255, 255, 255, a)

    img.save(output_datei, "PNG")


farben_aendern(
    input_datei,
    "output.png"
)