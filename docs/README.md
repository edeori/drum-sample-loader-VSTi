# Swarm — Tervezési dokumentáció

Ez a mappa a `drum-sample-loader-VSTi` hangszer-plugin funkcionális és architekturális
specifikációját tartalmazza. A cél egy Kontakt-szerű, multi-mikrofonos dob virtuális
hangszer, ahol minden dobtest/cintányér saját mikrofoncsatornákkal, velocity-sávokba
rendezett round-robin mintacsoportokkal és szabadon konfigurálható MIDI- és
audio-routinggal rendelkezik.

## Célplatform

- **Keretrendszer:** JUCE (C++)
- **Plugin formátumok:** VST3, AU (AAX opcionális, később)
- **Prioritás:** alacsony latency, natív RAM-alapú mintalejátszás

## Dokumentumtérkép

| Fájl | Tartalom |
|---|---|
| [01-architecture.md](01-architecture.md) | Rendszerarchitektúra, jelfolyam, low-latency elvek, multi-out busz felépítés |
| [02-instrument-model.md](02-instrument-model.md) | Alapértelmezett dobfelszerelés, hangszer adatmodell, mikrofoncsatorna-kategóriák, bővíthetőség |
| [03-editor-ui.md](03-editor-ui.md) | Hangszer-editor felület, globális velocity threshold beállítás, minta-böngésző, round-robin algoritmus, "let it ring" lejátszási modell |
| [04-mixing-routing.md](04-mixing-routing.md) | Faderek, csatorna-routing mátrix, közös overhead/room busz-csoport |
| [05-midi-mapping.md](05-midi-mapping.md) | MIDI note konfiguráció hangszerenként, alapértelmezett "Krimh-stílusú" note-tábla (placeholder) |
| [06-preset-format.md](06-preset-format.md) | Preset mentés/betöltés, fájlformátum vázlat |

## Alapvető funkcionális döntések (összefoglaló)

1. **Routing:** minden hangszer minden mikrofoncsatornája (spot, overhead, room) külön-külön
   irányítható tetszőleges plugin-kimeneti buszra, Kontakt-szerű routing mátrixszal.
2. **Velocity-sávok:** a threshold-ok és a sávok száma **globális** beállítás — egyszer
   állítjuk be, és minden hangszerre egyformán vonatkozik.
3. **Round-robin:** minden velocity-sávon belül tetszőleges számú mintakészlet (RR slot)
   rögzíthető; lejátszáskor véletlenszerű választás történik azzal a megkötéssel, hogy
   az előzőleg lejátszott slot nem ismétlődhet közvetlenül.
4. **Multi-mikrofonos leütés:** egy RR slot nem egyetlen fájl, hanem egy **fájlcsoport**
   (spot + overhead L/R + room L/R stb.) — egy valós leütés összes egyidejű mikrofon-
   felvétele, amit együtt, szinkronban játszunk le.
5. **Let it ring:** minden normál leütés önálló, teljes hosszban lejátszott hangforrás;
   egy új normál leütés nem szakítja meg az előzőt. Kivétel a Crash hangszerek külön
   MIDI note-on elérhető, explicit `Choke` artikulációja.
6. **Faderek:** hangszerenkénti, csatornánkénti fader (pl. Snare: Top/Bottom/OH/Room),
   plusz egy globális, minden hangszer overhead/room hozzájárulását együtt vezérlő
   master fader-csoport.
7. **Presetek:** a fader-állások, minta-hozzárendelések, routing és MIDI-map is része
   a menthető/betölthető presetnek.
8. **MIDI:** artikulációnként szabadon átkonfigurálható MIDI note; a Hi-hat
   `Closed`/`Half Open`/`Open`, a Crash hangszerek `Hit`/`Choke` artikulációt kapnak.
   Az alapértelmezett sablon egy "Krimh-stílusú" kiosztás (lásd [05-midi-mapping.md](05-midi-mapping.md)
   — **fontos védjegyzés a fájlban**, mert a pontos hivatalos note-lista nem volt
   megbízhatóan visszakereshető, ezért ott csak egy szerkeszthető placeholder szerepel).

## Nyitott kérdések / jövőbeli bővítési pontok

- További artikulációk (pl. snare rimshot, sidestick, hi-hat pedal) ugyanebben az
  artikuláció + külön MIDI note modellben később hozzáadhatók.
- Disk streaming nagy mintakönyvtárakhoz — az alapterv RAM-preload, de a
  formátum nem zárja ki későbbi streaming módot.
- A pontos, hivatalos Krimh Drums MIDI note-lista behelyettesítése a jelenlegi
  placeholder tábla helyére, amint rendelkezésre áll.
