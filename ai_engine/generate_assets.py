import os, math
from PIL import Image, ImageDraw
os.makedirs('plugin/Assets', exist_ok=True)
size = 512
img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
draw = ImageDraw.Draw(img)
c = size // 2
draw.rounded_rectangle([16, 16, size-16, size-16], radius=96, fill=(13, 15, 19, 255), outline=(45, 51, 64, 255), width=8)
draw.ellipse([64, 64, size-64, size-64], outline=(0, 229, 255, 220), width=10)
draw.ellipse([96, 96, size-96, size-96], fill=(18, 22, 30, 255), outline=(245, 158, 11, 180), width=4)
for i in range(-7, 8):
    x = c + i * 22
    h = int(110 * math.exp(-(i*i)/18.0) * abs(math.cos(i*0.6)) + 18)
    col = (245, 158, 11, 255) if abs(i) <= 2 else (0, 229, 255, 255)
    draw.rounded_rectangle([x-6, c-h, x+6, c+h], radius=5, fill=col)
img.save('plugin/Assets/icon.png')
img.save('plugin/Assets/icon.ico', format='ICO', sizes=[(256,256),(128,128),(64,64),(32,32)])
print('Icone generate con successo!')
