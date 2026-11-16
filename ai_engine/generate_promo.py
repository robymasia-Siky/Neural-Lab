import os, math
from PIL import Image, ImageDraw, ImageFont
os.makedirs('promo', exist_ok=True)
W, H = 1080, 1920
img = Image.new('RGB', (W, H), (10, 12, 16))
d = ImageDraw.Draw(img)
for y in range(0, H, 40): d.line([(0, y), (W, y)], fill=(18, 22, 30), width=1)
for x in range(0, W, 40): d.line([(x, 0), (x, H)], fill=(18, 22, 30), width=1)
d.rounded_rectangle([54, 90, W-54, H-90], radius=36, fill=(14, 17, 23), outline=(45, 51, 64), width=4)
d.rounded_rectangle([94, 140, W-94, 520], radius=24, fill=(9, 11, 15), outline=(0, 229, 255), width=3)
if os.path.exists('plugin/Assets/icon.png'):
    ic = Image.open('plugin/Assets/icon.png').resize((180, 180))
    img.paste(ic, (W//2 - 90, 175), ic)
try:
    f_title = ImageFont.truetype('arialbd.ttf', 74)
    f_sub = ImageFont.truetype('arialbd.ttf', 28)
    f_card = ImageFont.truetype('arialbd.ttf', 34)
    f_desc = ImageFont.truetype('arial.ttf', 26)
    f_cta = ImageFont.truetype('arialbd.ttf', 36)
except:
    f_title = f_sub = f_card = f_desc = f_cta = ImageFont.load_default()
d.text((W//2, 395), 'NEURAL LAB', fill=(248, 250, 252), font=f_title, anchor='mm')
d.text((W//2, 462), '100% LOCAL AI STUDIO & AUTO-MASTERING', fill=(0, 229, 255), font=f_sub, anchor='mm')
d.rounded_rectangle([94, 555, W-94, 830], radius=20, fill=(7, 9, 12), outline=(30, 36, 48), width=2)
for i in range(44):
    bx = 126 + i * 19
    bh = int(190 * abs(math.sin(i * 0.28 + 0.5)) * math.exp(-((i-18)**2)/320.0) + 24)
    col = (245, 158, 11) if i < 12 else (0, 229, 255)
    d.rounded_rectangle([bx, 805 - bh, bx + 13, 805], radius=4, fill=col)
features = [('0.5B MICRO-ORCHESTRATOR', 'Instant JSON arrangement & DAW routing (<50ms)'), ('LAION-CLAP AUDIO CORTEX', 'Semantic search across your local sample folders'), ('REAL-TIME C++ JUCE DSP', '64-bit Drive, Sub-Clean, Punch, Width & Limiter'), ('MATCHERING 2.0 & ABLETON', 'Reference track LUFS lock + auto clip loading')]
y0 = 875
for idx, (t, s) in enumerate(features):
    d.rounded_rectangle([94, y0, W-94, y0+155], radius=18, fill=(18, 22, 30), outline=(42, 48, 60), width=2)
    acc = (245, 158, 11) if idx % 2 else (0, 229, 255)
    d.rounded_rectangle([114, y0+26, 124, y0+129], radius=4, fill=acc)
    d.text((150, y0+52), t, fill=(241, 245, 249), font=f_card, anchor='lm')
    d.text((150, y0+106), s, fill=(148, 163, 184), font=f_desc, anchor='lm')
    y0 += 180
d.rounded_rectangle([94, 1625, W-94, 1775], radius=24, fill=(0, 95, 115), outline=(0, 229, 255), width=3)
d.text((W//2, 1678), 'OPEN SOURCE ON GITHUB', fill=(255, 255, 255), font=f_cta, anchor='mm')
d.text((W//2, 1728), 'github.com/robymasia-Siky/Neural-Lab', fill=(251, 191, 36), font=f_sub, anchor='mm')
img.save('promo/promo_tiktok_cover.png')
print('Copertina promo generata: promo/promo_tiktok_cover.png')
