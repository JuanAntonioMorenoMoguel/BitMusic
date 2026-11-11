# Generates the example songs in assets/examples/ (original compositions): python3 tools/make_examples.py assets/examples
import json, sys, os
OUT = sys.argv[1]

def seq(text, length):
    notes = ["" if t == "." else t for t in text.split()]
    assert len(notes) <= length, (text, len(notes), length)
    while notes and notes[-1] == "": notes.pop()
    return notes

def at(length, events):
    notes = [""] * length
    for row, note in events.items(): notes[row] = note
    while notes and notes[-1] == "": notes.pop()
    return notes

def arp(chords, length, octave_notes):
    # one chord per 16 rows, cycling its tones every row
    out = []
    for r in range(length):
        tones = octave_notes[chords[r // 16 % len(chords)]]
        out.append(tones[r % len(tones)] + (":0.7" if r % 2 else ""))
    return out

def pattern(length, tracks):
    return {"Length": length, "Tracks": [{"Instrument": i, "Notes": n} for i, n in tracks]}

def song(name, bpm, rows, beats, volume, loop, instruments, patterns, order):
    data = {"Format": "bitmusic", "Version": 1, "Bpm": bpm, "Rows": rows, "Beats": beats, "Volume": volume,
            "Loop": loop, "Instruments": instruments, "Patterns": patterns, "Order": order}
    for p in patterns.values():
        for t in p["Tracks"]:
            assert t["Instrument"] in instruments, t["Instrument"]
            assert len(t["Notes"]) <= p["Length"]
    for o in order: assert o in patterns, o
    with open(os.path.join(OUT, name + ".bitmusic"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")

kick = {"Wave": "sine", "Volume": 0.8, "Attack": 0.001, "Decay": 0.16, "Sustain": 0, "Release": 0.03, "Sweep": -24, "SweepTime": 0.07}
snare = {"Wave": "noise", "Volume": 0.35, "Attack": 0.001, "Decay": 0.13, "Sustain": 0, "Release": 0.05, "LowPass": 0.55}
hat = {"Wave": "noise", "Volume": 0.16, "Attack": 0.001, "Decay": 0.035, "Sustain": 0, "Release": 0.01, "Pan": 0.25}

# ---------------------------------------------------------------- 1. Aventura: cheerful chiptune, C major, 140 BPM
four = "C3 . . . C3 . . . C3 . . . C3 . . ."
snr = ". . . . C4 . . . . . . . C4 . . ."
hats = ". . C7 . . . C7:0.6 . . . C7 . . . C7:0.6 C7:0.4"
def bars(*parts): return " ".join(parts)
chords = {"C": ["C5", "E5", "G5", "E5"], "Am": ["A4", "C5", "E5", "C5"], "F": ["F4", "A4", "C5", "A4"], "G": ["G4", "B4", "D5", "B4"]}
song("aventura", 140, 4, 4, 0.45, 1, {
    "kick": kick, "snare": snare, "hihat": hat,
    "bass": {"Wave": "triangle", "Volume": 0.6, "Attack": 0.005, "Decay": 0.15, "Sustain": 0.6, "Release": 0.06},
    "lead": {"Wave": "square", "Width": 0.25, "Volume": 0.24, "Attack": 0.01, "Decay": 0.1, "Sustain": 0.7, "Release": 0.12, "Pan": -0.2},
    "arp": {"Wave": "square", "Width": 0.125, "Volume": 0.1, "Attack": 0.002, "Decay": 0.06, "Sustain": 0.4, "Release": 0.04, "Pan": 0.35},
}, {
    "intro": pattern(64, [
        ("kick", seq(bars(four, four, four, "C3 . . . C3 . . . C3 . C3 . C3 C3 C3 C3"), 64)),
        ("snare", seq(bars(snr, snr, snr, ". . . . C4 . . . C4:0.5 . C4:0.7 . C4 C4:0.6 C4:0.8 C4"), 64)),
        ("hihat", seq(bars(hats, hats, hats, hats), 64)),
        ("bass", seq(bars("C2 . C3 . C2 . C3 . C2 . C3 . C2 . C3 .", "A1 . A2 . A1 . A2 . A1 . A2 . A1 . A2 .",
                          "F1 . F2 . F1 . F2 . F1 . F2 . F1 . F2 .", "G1 . G2 . G1 . G2 . G1 . B1 . D2 . G2 ."), 64)),
        ("lead", []), ("arp", []),
    ]),
    "tema": pattern(64, [
        ("kick", seq(bars(four, four, four, four), 64)),
        ("snare", seq(bars(snr, snr, snr, snr), 64)),
        ("hihat", seq(bars(hats, hats, hats, hats), 64)),
        ("bass", seq(bars("C2 . C3 . C2 . C3 . C2 . C3 . C2 . C3 .", "A1 . A2 . A1 . A2 . A1 . A2 . A1 . A2 .",
                          "F1 . F2 . F1 . F2 . F1 . F2 . F1 . F2 .", "G1 . G2 . G1 . G2 . G1 . B1 . D2 . G2 ."), 64)),
        ("lead", seq(bars("E5 . . . G5 . . . C6 . . B5 . . G5 .", "A5 . . . E5 . . . C5 . . . . . off .",
                          "F5 . . . A5 . . . C6 . . A5 . . F5 .", "G5 . . . . . D5 . G5 . B5 . D6 . . off"), 64)),
        ("arp", arp(["C", "Am", "F", "G"], 64, chords)),
    ]),
    "puente": pattern(64, [
        ("kick", seq(bars(four, four, four, "C3 . . . C3 . . . C3 . C3 . C3 C3 C3 C3"), 64)),
        ("snare", seq(bars(snr, snr, snr, ". . . . C4 . . . C4:0.5 . C4:0.7 . C4 C4:0.6 C4:0.8 C4"), 64)),
        ("hihat", seq(bars(hats, hats, hats, hats), 64)),
        ("bass", seq(bars("F1 . F2 . F1 . F2 . F1 . F2 . F1 . F2 .", "G1 . G2 . G1 . G2 . G1 . G2 . G1 . G2 .",
                          "E1 . E2 . E1 . E2 . A1 . A2 . A1 . A2 .", "D2 . D3 . D2 . D3 . G1 . G2 . G1 . B1 ."), 64)),
        ("lead", seq(bars("A5 . G5 . F5 . . . C5 . D5 . F5 . . .", "G5 . F5 . E5 . . . D5 . . . B4 . . .",
                          "E5 . . . G#5 . . . A5 . . . C6 . . .", "B5 . . A5 . . G5 . . F5 . . D5 . off ."), 64)),
        ("arp", arp(["F", "G", "Am", "G"], 64, chords)),
    ]),
}, ["intro", "tema", "tema", "puente", "tema"])

# ---------------------------------------------------------------- 2. Mazmorra: slow, dark, D minor in 3/4, 84 BPM
song("mazmorra", 84, 4, 3, 0.75, 0, {
    "pad": {"Wave": "saw", "Volume": 0.16, "Attack": 0.6, "Decay": 0.8, "Sustain": 0.7, "Release": 1.2, "LowPass": 0.12},
    "pad2": {"Wave": "saw", "Volume": 0.12, "Attack": 0.7, "Decay": 0.8, "Sustain": 0.7, "Release": 1.2, "LowPass": 0.1, "Pan": 0.5, "Transpose": 0.1},
    "bass": {"Wave": "sine", "Volume": 0.55, "Attack": 0.02, "Decay": 0.5, "Sustain": 0.5, "Release": 0.4},
    "bell": {"Wave": "triangle", "Volume": 0.22, "Attack": 0.001, "Decay": 0.6, "Sustain": 0, "Release": 0.6, "Sweep": 0.3, "SweepTime": 0.2, "Pan": -0.4},
    "drip": {"Wave": "sine", "Volume": 0.18, "Attack": 0.001, "Decay": 0.08, "Sustain": 0, "Release": 0.05, "Sweep": 12, "SweepTime": 0.05, "Pan": 0.6},
}, {
    "cueva": pattern(48, [
        ("pad", seq("D4 . . . . . . . . . . . . . . . . . . . . . . . Bb3 . . . . . . . . . . . . . . . . . . . . . . .", 48)),
        ("pad2", seq("F4 . . . . . . . . . . . . . . . . . . . . . . . D4 . . . . . . . . . . . . . . . . . . . . . . .", 48)),
        ("bass", seq("D2 . . . . . . . . . . . A1 . . . . . . . . . . . Bb1 . . . . . . . . . . . A1 . . . . . . . . . . .", 48)),
        ("bell", seq("A5 . . . . . F5 . . . . . E5 . . . . . . . . . . . D5 . . . . . F5 . . . . . A5 . . . . . G5 . . . . .", 48)),
        ("drip", at(48, {22: "C7:0.5", 46: "G6:0.3"})),
    ]),
    "eco": pattern(48, [
        ("pad", seq("G3 . . . . . . . . . . . . . . . . . . . . . . . A3 . . . . . . . . . . . . . . . . . . . . . . .", 48)),
        ("pad2", seq("Bb3 . . . . . . . . . . . . . . . . . . . . . . . C#4 . . . . . . . . . . . . . . . . . . . . . . .", 48)),
        ("bass", seq("G1 . . . . . . . . . . . G1 . . . . . . . . . . . A1 . . . . . . . . . . . A1 . . . A1 . . . A1 . . .", 48)),
        ("bell", seq("D6 . . . . . C6 . . . . . Bb5 . . . . . . . . . . . A5 . . . . . G5 . . . . . E5 . . . . . C#5 . . . . .", 48)),
        ("drip", at(48, {14: "E7:0.4", 44: "C7:0.5"})),
    ]),
}, ["cueva", "eco", "cueva", "eco"])

# ---------------------------------------------------------------- 3. Jefe: boss battle, E minor, 172 BPM, crushed
gallop = "E2 . E2 E2 E2 . E2 E2 E2 . E2 E2 E2 . E2 E2"
song("jefe", 172, 4, 4, 0.48, 0, {
    "kick": dict(kick, Volume=0.9), "snare": dict(snare, Volume=0.4, Crush=6),
    "bass": {"Wave": "square", "Width": 0.5, "Volume": 0.3, "Attack": 0.002, "Decay": 0.08, "Sustain": 0.5, "Release": 0.03, "LowPass": 0.3},
    "lead": {"Wave": "saw", "Volume": 0.2, "Attack": 0.005, "Decay": 0.1, "Sustain": 0.8, "Release": 0.08, "Crush": 10, "Downsample": 2, "Pan": -0.15},
    "alarm": {"Wave": "square", "Width": 0.125, "Volume": 0.12, "Attack": 0.001, "Decay": 0.2, "Sustain": 0.3, "Release": 0.05, "Sweep": -12, "SweepTime": 0.15, "Pan": 0.4},
}, {
    "ataque": pattern(32, [
        ("kick", seq("C3 . . . C3 . . C3 C3 . . . C3 . . . C3 . . . C3 . . C3 C3 . . . C3 . C3 C3", 32)),
        ("snare", seq(". . . . C4 . . . . . . . C4 . . . . . . . C4 . . . . . . . C4 C4:0.6 C4:0.8 C4", 32)),
        ("bass", seq(gallop + " " + "C2 . C2 C2 C2 . C2 C2 D2 . D2 D2 D2 . D2 D2", 32)),
        ("lead", seq("E5 . . . . . B4 . E5 . F#5 . G5 . . . A5 . G5 . F#5 . . . D5 . . . B4 . off .", 32)),
        ("alarm", seq("E6 . . . . . . . . . . . . . . . . . . . . . . . . . . . E6:0.6 . . .", 32)),
    ]),
    "furia": pattern(32, [
        ("kick", seq("C3 . C3 . C3 . C3 . C3 . C3 . C3 . C3 . C3 . C3 . C3 . C3 . C3 C3 C3 C3 C3 C3 C3 C3", 32)),
        ("snare", seq(". . . . C4 . . . . . . . C4 . . . . . . . C4 . . . C4:0.4 C4:0.5 C4:0.6 C4:0.7 C4:0.8 C4:0.9 C4 C4", 32)),
        ("bass", seq("A1 . A1 A1 A1 . A1 A1 A1 . A1 A1 A1 . A1 A1 B1 . B1 B1 B1 . B1 B1 B1 . B1 B1 D#2 . F#2 .", 32)),
        ("lead", seq("C6 . B5 . A5 . . . E5 . . . A5 . C6 . B5 . . . F#5 . . . D#5 . F#5 . B5 . . off", 32)),
        ("alarm", seq(". . . . . . . . E6 . . . . . . . . . . . . . . . B5 . . . B5 . B5 .", 32)),
    ]),
}, ["ataque", "ataque", "furia", "ataque", "furia", "furia"])

# ---------------------------------------------------------------- 4. Victoria: short fanfare that stops (Loop -1)
song("victoria", 120, 4, 4, 0.56, -1, {
    "brass": {"Wave": "saw", "Volume": 0.22, "Attack": 0.02, "Decay": 0.15, "Sustain": 0.75, "Release": 0.3, "LowPass": 0.4},
    "brass2": {"Wave": "square", "Width": 0.3, "Volume": 0.14, "Attack": 0.02, "Decay": 0.15, "Sustain": 0.7, "Release": 0.3, "Pan": 0.4},
    "bass": {"Wave": "triangle", "Volume": 0.6, "Attack": 0.005, "Decay": 0.2, "Sustain": 0.6, "Release": 0.3},
    "tom": {"Wave": "sine", "Volume": 0.6, "Attack": 0.001, "Decay": 0.25, "Sustain": 0, "Release": 0.1, "Sweep": -7, "SweepTime": 0.2},
    "cymbal": {"Wave": "noise", "Volume": 0.2, "Attack": 0.001, "Decay": 1.2, "Sustain": 0, "Release": 0.8, "LowPass": 0.8},
}, {
    "fanfarria": pattern(32, [
        ("brass", seq("G4 . G4 . G4 . . . Eb5 . . . F5 . . . G5 . . . . . F5 . G5 . . . . . . off", 32)),
        ("brass2", seq("D4 . D4 . D4 . . . G4 . . . A4 . . . B4 . . . . . A4 . B4 . . . . . . off", 32)),
        ("bass", seq("G2 . . . . . . . Eb2 . . . F2 . . . G2 . . . . . . . G1 . . . . . . off", 32)),
        ("tom", seq("C3 . C3 . C3 . . . C3 . . . C3 . . . C2 . . . . . . . C2 . . . . . . .", 32)),
        ("cymbal", seq(". . . . . . . . . . . . . . . . C6 . . . . . . . C6 . . . . . . .", 32)),
    ]),
}, ["fanfarria"])

# ---------------------------------------------------------------- 5. Prueba del sintetizador: every feature once
waves = {}
for w in ["sine", "square", "saw", "triangle", "noise"]:
    waves[w] = {"Wave": w, "Volume": 0.3, "Attack": 0.01, "Decay": 0.1, "Sustain": 0.6, "Release": 0.1}
song("prueba-sinte", 100, 4, 4, 0.8, -1, dict(waves, **{
    "pulse12": {"Wave": "square", "Width": 0.125, "Volume": 0.25},
    "crushed": {"Wave": "triangle", "Volume": 0.35, "Crush": 4, "Downsample": 8},
    "dark": {"Wave": "saw", "Volume": 0.35, "LowPass": 0.05},
    "left": {"Wave": "sine", "Volume": 0.4, "Pan": -1}, "right": {"Wave": "sine", "Volume": 0.4, "Pan": 1},
    "upsweep": {"Wave": "square", "Volume": 0.2, "Sweep": 24, "SweepTime": 0.4, "Sustain": 1},
    "fifth": {"Wave": "triangle", "Volume": 0.35, "Transpose": 7},
}), {
    "ondas": pattern(32, [
        ("sine", seq("C4 . . off . . . . . . . . . . . . . . . . . . . . . . . . . . . .", 32)),
        ("square", seq(". . . . C4 . . off . . . . . . . . . . . . . . . . . . . . . . . .", 32)),
        ("saw", seq(". . . . . . . . C4 . . off . . . . . . . . . . . . . . . . . . . .", 32)),
        ("triangle", seq(". . . . . . . . . . . . C4 . . off . . . . . . . . . . . . . . . .", 32)),
        ("noise", seq(". . . . . . . . . . . . . . . . C4 . . off . . . . . . . . . . . .", 32)),
        ("pulse12", seq(". . . . . . . . . . . . . . . . . . . . C4 . . off . . . . . . . .", 32)),
        ("crushed", seq(". . . . . . . . . . . . . . . . . . . . . . . . C4 . . off . . . .", 32)),
        ("dark", seq(". . . . . . . . . . . . . . . . . . . . . . . . . . . . C4 . . off", 32)),
    ]),
    "detalles": pattern(32, [
        ("left", seq("A4 . . off . . . . . . . . . . . . . . . . . . . . . . . . . . . .", 32)),
        ("right", seq(". . . . A4 . . off . . . . . . . . . . . . . . . . . . . . . . . .", 32)),
        ("upsweep", seq(". . . . . . . . C3 . . . . . off . . . . . . . . . . . . . . . . .", 32)),
        ("fifth", seq(". . . . . . . . . . . . . . . . C4 . --- . E4 . --- . G4 . . . . . off .", 32)),
        # volumes, a MIDI number with a fraction (a quarter tone), and the other "keep" spellings
        ("sine", seq(". . . . . . . . . . . . . . . . C5:0.25 . C5:0.5 . C5:0.75 . C5 . 72 . 72.5 . 73 . off .", 32)),
    ]),
}, ["ondas", "detalles"])
print("ok")
