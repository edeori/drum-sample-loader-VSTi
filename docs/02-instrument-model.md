# 2. Hangszermodell

## 2.1 Alapértelmezett dobfelszerelés

A plugin egy előre definiált default kittel indul, amely szabadon bővíthető:

| Hangszer | Kategória | Alapértelmezett mikrofoncsatornák |
|---|---|---|
| Kick | Test (body) | In (spot), Out (spot), Overhead L/R, Room L/R |
| Snare | Test (body) | Top (spot), Bottom (spot), Overhead L/R, Room L/R |
| Tom 1–4 | Test (body) | Spot, Overhead L/R, Room L/R |
| Hi-hat | Cintányér (cymbal) | Spot, Overhead L/R, Room L/R |
| Crash 1 | Cintányér | Spot, Overhead L/R, Room L/R |
| Crash 2 | Cintányér | Spot, Overhead L/R, Room L/R |
| Ride | Cintányér | Spot, Overhead L/R, Room L/R |
| China | Cintányér | Spot, Overhead L/R, Room L/R |
| Splash | Cintányér | Spot, Overhead L/R, Room L/R |

> A táblázat az alapkészletet mutatja — a kit szabadon bővíthető további
> cintányérokkal (pl. Crash 3, Splash 2, Stack, egyedi elnevezésű percussion)
> az editor "Add Instrument" funkciójával (lásd [03-editor-ui.md](03-editor-ui.md)).

## 2.2 Kategóriák és mikrofon-sablonok

Két hangszerkategória van, ami meghatározza az **alapértelmezett** mikrofoncsatorna-
készletet új hangszer felvételekor:

- **Body (dobtest):** kap spot mikrofon(oka)t + megosztott Overhead és Room csatornákat.
  A spot mikrofonok száma hangszerenként eltérő lehet (Kick: 2, Snare: 2, Tom: 1).
- **Cymbal (cintányér):** alapból egy Spot, valamint Overhead és Room csatornákat
  kap. A Spot cellák üresen maradhatnak, ha az adott felvételhez nem készült spot
  mikrofon. Ez a Hi-hat, Crash, Ride, China és minden további cintányér sablonjára
  egységesen érvényes.
- Az Overhead és Room forrás külön-külön lehet két mono `L/R` csatorna vagy egy
  interleaved stereo csatorna. A csatornalista és a minta-rács a kiválasztott
  formátumhoz igazodik. A sample packben található további mono ambience
  mikrofonok (például `Room M`) kiegészítő csatornaként megmaradnak.

## 2.3 Hangszer adatmodell

```text
Instrument
├─ id                     (stabil egyedi azonosító, preset-kompatibilitáshoz)
├─ name                   (pl. "Snare", "Crash 2", felhasználó átnevezheti)
├─ category               (Body | Cymbal)
├─ articulations[]         (legalább egy "Main" artikuláció)
│  ├─ name                 (pl. Closed, Half Open, Hit, Choke, Bell)
│  ├─ midiNote             (artikulációnként konfigurálható)
│  ├─ action               (Trigger | Choke)
│  └─ velocityLayers[]     ← a globális threshold-konfiguráció szerinti sávok
│       └─ roundRobinSlots[]
│            └─ { micChannel → sampleFilePath } map
├─ micChannels[]           (pl. ["Top", "Bottom", "OH_L", "OH_R", "Room_L", "Room_R"])
├─ ambienceFormat          (Overhead és Room: MonoPair | Stereo)
├─ channelFaders            (micChannel → gain/dB, pl. Top=0dB, Bottom=-3dB, OH=-6dB)
├─ channelRouting           (micChannel → kimeneti busz azonosító)
└─ lastPlayedSlotPerLayer   (futásidejű állapot a round-robin "nem ismétlődhet" szabályhoz,
                             nem menthető preset-adat)
```

Fontos, hogy a **velocity-sávok száma és határai nem hangszer-szintű adat**, hanem
a globális konfigurációra mutató hivatkozás — lásd [03-editor-ui.md](03-editor-ui.md)
2. pont. Ez biztosítja, hogy egy threshold-módosítás egyszerre minden hangszerre
érvényesüljön.

## 2.4 Bővíthetőség

- Új hangszer hozzáadása: kategória kiválasztása (Body/Cymbal) → alapértelmezett
  mikrofoncsatorna-sablon behelyezése → MIDI note hozzárendelése (ütközés-ellenőrzéssel
  a már foglalt note-okhoz képest) → azonnal szerkeszthető a normál editoron.
- Mikrofoncsatorna hozzáadása/eltávolítása egy meglévő hangszerhez bármikor lehetséges;
  ez nem törli a már felvett mintákat a többi csatornán.
- Hangszer törlése/duplikálása (pl. "Crash 2" mint kiindulási sablon egy új "Crash 3"-hoz).
