# Swarm – drum sample loader VSTi

Natív JUCE VST3/AU/Standalone dob-sample loader alkalmazás, a `mockup/` mappában
megtartott interaktív HTML látványtervvel.

## Natív alkalmazás build

```bash
cmake -S . -B build
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

## Megnyitás

Az `mockup/index.html` közvetlenül megnyitható böngészőben, vagy helyi webszerverrel:

```bash
python3 -m http.server 4173 --directory mockup
```

Ezután: `http://localhost:4173`

## Interaktív elemek

- hangszerkiválasztás, keresés, mute/solo és új hangszer hozzáadása;
- Hi-hat és Crash artikulációk külön MIDI note-okkal;
- velocity-rétegek és globális threshold értékek módosítása;
- round-robin slotok és helyi WAV/AIFF fájlok betöltése;
- minden hangszerhez opcionálisan használható Spot csatorna;
- külön L/R vagy egyetlen stereo Overhead/Room csatornaformátum;
- csatornafaderek, hard-pan kijelzés, routing és master beállítások;
- fader nullázás dupla kattintással vagy `Cmd/Ctrl` + kattintással;
- MIDI note kiosztás és MIDI Learn állapot;
- animált preview- és master kivezérlésmérők;
- billentyűzet: `Cmd/Ctrl + K` keresés, `Space` preview.

## Képi assetek

A `mockup/assets/instruments/` mappa különálló, átlátszó PNG-ként tartalmazza a
kick, snare, tom és cymbal metszeteket. Az eredeti, szerkeszthető assetlap és a
chroma-key forrás a `mockup/assets/` mappában található.
