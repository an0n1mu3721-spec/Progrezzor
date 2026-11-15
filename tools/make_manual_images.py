#!/usr/bin/env python3
"""Makes the screenshots for the manual (docs/manual/img/*.png).

1. Runs the UI test, which saves real snapshots of the plug-in window.
2. Draws the arrows and labels on them (Pillow only), onto a dark background.

Run from the project root, after building the UI test:
    cmake --build build --target ProgrezzorUiTest
    python3 tools/make_manual_images.py

For clean screenshots build from a clean checkout (no "-dirty" next to the version).
Needs Pillow (pip install pillow). Do not edit the PNGs by hand: change the lists below and run again.
"""
import math, subprocess, sys, tempfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
UITEST = ROOT / "build/ProgrezzorUiTest_artefacts/Release/ProgrezzorUiTest"
OUT = ROOT / "docs/manual/img"
FONT = ROOT / "resources/fonts/Inter/Inter-SemiBold.otf"
BG, PANEL, FG, ORANGE, DARK = (18, 16, 31), (27, 24, 48), (235, 233, 247), (255, 180, 84), (26, 18, 48)
S = 3  # drawing is done 3 times larger and scaled down, so lines are smooth

# Each picture: output name -> (snapshot name, margins (left, top, right, bottom), font size, [(label, (x, y) of label, [points the arrow goes through, last = tip])]).
# Coordinates are in pixels of the snapshot. A label that is only digits becomes a numbered circle.
P = (0, 44, 150, 44)
PICS = {
 "overview": ("bar_suggested", (48, 48, 48, 48), 19, [(a, b, [c]) for a, b, c in [
  ("1",(255,-26),(255,14)),("2",(319,-26),(319,14)),("3",(388,-26),(388,14)),("4",(460,-26),(460,14)),("5",(543,-26),(543,14)),("6",(667,-26),(667,14)),
  ("7",(836,-26),(783,14)),("7",(836,-26),(889,14)),("8",(1018,-26),(1018,14)),("9",(1126,66),(1090,66)),("10",(-26,183),(13,183)),("11",(1126,183),(1087,183)),
  ("12",(-26,332),(26,332)),("13",(-26,397),(26,397)),("14",(-26,457),(26,457)),("15",(440,586),(440,539)),
  ("16",(665,319),(784,319)),("17",(905,288),(944,310)),("18",(665,350),(784,354)),("20",(665,398),(784,391)),("21",(665,427),(784,427)),
  ("22",(1000,586),(1000,485)),("23",(1126,372),(1077,372))]] + [("19",(710,372),[(963,372),(963,367)])]),
 "pad-lit": ("pad3_lit", (0,0,0,0), 19, [("Lit pad",(365,82),[(365,112)])]),
 "midi-lit": ("midi_two_lit", (0,0,0,0), 19, [("Lit",(140,86),[(98,114)]),("Lit",(650,86),[(650,113)]),("Notes playing",(850,522),[(862,484)])]),
 "key-d-major": ("d_major", (0,0,0,0), 19, [("Key",(255,78),[(255,52)]),("Scale",(388,78),[(388,52)])]),
 "key-bb-minor": ("bb_minor", (0,0,0,0), 19, [("Key",(255,78),[(255,52)]),("Scale",(388,78),[(388,52)])]),
 "outside-key": ("outside_marked", (0,0,0,0), 19, [("Outside the key: silent",(300,445),[(300,400)])]),
 "suggest": ("bar_suggested", (0,0,0,0), 19, [("Suggest",(870,72),[(960,46)]),("4 to 8 chords",(1005,98),[(1034,74)])]),
 "suggest-asks": ("suggest_asks", (0,0,0,0), 19, [("Replace",(658,495),[(658,450)])]),
 "drag-replace": ("drag_replace", (0,0,0,0), 19, [("Block to replace",(430,445),[(430,400)])]),
 "resize": ("resize_long", (0,0,0,0), 19, [("Drag the edge",(560,450),[(604,370)])]),
 "block-controls": ("block_gear", (0,0,0,0), 19, [("Padlock",(300,300),[(256,337)]),("Octave",(410,300),[(345,334)]),("Menu",(185,440),[(248,386)])]),
 "locked-block": ("locked_block", (0,0,0,0), 19, [("Locked",(310,300),[(257,336)])]),
 "octaves": ("octave_blocks", (0,0,0,0), 19, [("+1 octave",(196,445),[(196,388)]),("-1 octave",(330,445),[(330,388)])]),
 "rest": ("rest_in_bar", (0,0,0,0), 19, [("Rest",(300,445),[(300,398)])]),
 "rest-lengths": ("rest_lengths", (0,0,0,0), 19, [("Rest symbol",(690,455),[(655,390)])]),
 "playing": ("loop_playing", (0,0,0,0), 19, [("Playing now",(300,445),[(300,404)])]),
 "add-on": ("rec_on", (0,0,0,0), 19, [("Add is on",(700,365),[(785,324)])]),
 "arp-on": ("arp_on", (0,0,0,0), 19, [("Arp on",(160,470),[(66,411)]),("Arp settings",(195,425),[(90,402)])]),
 "arp-panel": ("arp_panel", P, 18, [("Hold, Arp or Rhythm",(185,-22),[(185,8)]),("Pattern",(445,94),[(358,94)]),("Speed",(445,172),[(358,172)]),("Presets",(445,257),[(358,257)]),("Turn the ring",(68,395),[(68,346)])]),
 "rhythm-panel": ("rhythm_panel", P, 18, [("Rhythm is on",(300,-22),[(300,8)]),("12 patterns",(445,138),[(358,138)]),("Speed",(75,306),[(75,284)]),("Length",(255,306),[(255,284)]),("Preview",(445,342),[(358,342)])]),
 "quicktext": ("quicktext_typed", (0,0,0,0), 19, [("Type here",(450,470),[(240,395)])]),
 "quicktext-typo": ("quicktext_typo", (0,0,0,0), 19, [("Not a chord",(300,450),[(172,396)])]),
 "chordpro": ("chordpro_text", (0,0,0,0), 19, [("Chords from the file",(470,450),[(300,394)])]),
 "chordpro-key": ("chordpro_key", (0,0,0,48), 19, [("Switch the key",(578,582),[(578,486)])]),
 "key-guess": ("keyguess_ask", (0,0,0,48), 19, [("Switch the key",(578,582),[(578,508)]),("Doesn't fit",(300,300),[(300,327)])]),
 "learn": ("learn_hint", (0,0,0,0), 19, [("Right",(275,420),[(224,352)]),("Wrong: red edge",(620,365),[(494,362)]),("Hint",(738,470),[(698,485)])]),
 "saved": ("export", (0,0,0,0), 19, [("Saved",(783,80),[(783,52)])]),
 "sound-on": ("sound_on", (0,0,0,0), 19, [("Preview sound",(667,80),[(667,52)])]),
 "sound-off": ("sound_off", (0,0,0,0), 19, [("Off",(667,80),[(667,52)])]),
 "wheel": ("wheel_c_major", (0,0,0,0), 19, [("Chord wheel",(570,88),[(338,50)])]),
 "wheel-minor": ("wheel_minor", (0,0,0,0), 19, [("Chord wheel",(570,88),[(338,50)])]),
}

def edge(cx, cy, w, h, tx, ty):
    dx, dy = tx - cx, ty - cy
    k = min((w / 2) / abs(dx) if dx else 1e9, (h / 2) / abs(dy) if dy else 1e9)
    return cx + dx * k, cy + dy * k

def draw(shot, margins, fs, notes):
    ml, mt, mr, mb = margins
    iw, ih = shot.size
    W, H = ml + iw + mr, mt + ih + mb
    canvas = Image.new("RGB", (W, H), BG)
    shot = shot.convert("RGBA")
    canvas.paste(shot, (ml, mt), shot)  # transparent corners take the dark background
    ov = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    font = ImageFont.truetype(str(FONT), fs * S)
    nfont = ImageFont.truetype(str(FONT), int(fs * 0.9 * S))
    labels = []
    for text, (tx, ty), pts in notes:
        cx, cy = tx + ml, ty + mt
        pts = [(x + ml, y + mt) for x, y in pts]
        if text.isdigit(): w = h = fs * 1.45
        else: w, h = font.getlength(text) / S + 20, fs + 11
        sx, sy = edge(cx, cy, w, h, *pts[0])
        line = [(sx, sy)] + pts
        # shorten the last segment so the line meets the arrow head
        (px, py), (qx, qy) = line[-2], line[-1]
        L = math.hypot(qx - px, qy - py) or 1
        ux, uy = (qx - px) / L, (qy - py) / L
        head = fs * 0.8
        line[-1] = (qx - ux * head * 0.7, qy - uy * head * 0.7)
        sc = [(x * S, y * S) for x, y in line]
        d.line(sc, fill=DARK + (255,), width=int(fs * 0.3 * S), joint="curve")
        d.line(sc, fill=ORANGE + (255,), width=int(fs * 0.14 * S), joint="curve")
        nx, ny = -uy, ux
        tri = lambda k: [((qx + ux * 1.2 * k) * S, (qy + uy * 1.2 * k) * S),
                         ((qx - ux * head - nx * head * 0.55 * k) * S, (qy - uy * head - ny * head * 0.55 * k) * S),
                         ((qx - ux * head + nx * head * 0.55 * k) * S, (qy - uy * head + ny * head * 0.55 * k) * S)]
        d.polygon(tri(1.35), fill=DARK + (255,))
        d.polygon(tri(1.0), fill=ORANGE + (255,))
        labels.append((text, cx, cy, w, h))
    for text, cx, cy, w, h in labels:
        if text.isdigit():
            d.ellipse([(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S], fill=ORANGE + (255,), outline=DARK + (255,), width=int(2.5 * S))
            d.text((cx * S, cy * S), text, font=nfont, fill=DARK + (255,), anchor="mm")
        else:
            d.rounded_rectangle([(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S], radius=h / 2 * S, fill=PANEL + (255,), outline=ORANGE + (255,), width=2 * S)
            d.text((cx * S, cy * S), text, font=font, fill=FG + (255,), anchor="mm")
    ov = ov.resize((W, H), Image.LANCZOS)
    canvas.paste(ov, (0, 0), ov)
    return canvas

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    if not UITEST.exists(): sys.exit(f"Build the UI test first: {UITEST} is missing")
    with tempfile.TemporaryDirectory() as tmp:
        r = subprocess.run([str(UITEST), f"{tmp}/s"], capture_output=True, text=True)
        if "ALL OK" not in r.stdout: sys.exit("The UI test failed:\n" + r.stdout[-2000:])
        for name, (snap, margins, fs, notes) in PICS.items():
            img = draw(Image.open(f"{tmp}/s_{snap}.png"), margins, fs, notes)
            img.save(OUT / f"{name}.png", optimize=True)
            print("wrote", name)

if __name__ == "__main__":
    main()
