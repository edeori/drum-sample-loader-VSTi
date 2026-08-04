# 6. Preset rendszer

## 6.1 Mit ment el egy preset

Egy kit-preset **teljes, önmagában betölthető** állapotot tárol:

- a hangszerek listája (id, név, kategória, mikrofoncsatorna-lista, artikulációk és MIDI note-ok)
- az Overhead és Room csatornaformátuma (`MonoPair` vagy `Stereo`)
- a globális velocity threshold konfiguráció (sávok száma + határok)
- artikulációnként és velocity-sávonként a round-robin slot-ok és a hozzájuk tartozó
  mintafájl-elérési utak (mikrofoncsatornánként)
- hangszerenkénti csatorna-faderek (4.1)
- kimeneti routing mátrix (4.2)
- Master Overhead / Master Room / Master fader (4.3, 4.4)
- MIDI-map (5.1)

## 6.2 Fájlformátum (vázlat)

JSON, ember által is olvasható/verziózható struktúra:

```jsonc
{
  "presetVersion": 2,
  "kitName": "My Metal Kit v1",
  "globalVelocityLayers": {
    "layerNames": ["Ghost", "Soft", "Medium", "Heavy"],
    "thresholds": [32, 64, 96]   // N-1 határ N sávhoz, 0-127 skálán
  },
  "masterFaders": {
    "master_dB": 0.0,
    "masterOverhead_dB": -3.0,
    "masterRoom_dB": -6.0
  },
  "instruments": [
    {
      "id": "snare",
      "name": "Snare",
      "category": "Body",
      "articulations": [{
        "id": "main",
        "name": "Main",
        "midiNote": 38,
        "action": "Trigger",
        "velocityLayers": [
          {
            "layerIndex": 3,
            "roundRobinSlots": [
              { "Top": "samples/snare/heavy_hit1_top.wav", "OH_L": "samples/snare/heavy_hit1_ohl.wav", "OH_R": "samples/snare/heavy_hit1_ohr.wav" }
            ]
          }
        ]
      }],
      "micChannels": [
        { "id": "Top", "displayName": "Top (Spot)", "role": "Spot", "numChannels": 1, "pan": 0.0 },
        { "id": "OH_L", "displayName": "Overhead L", "role": "Overhead", "numChannels": 1, "pan": -1.0 },
        { "id": "OH_R", "displayName": "Overhead R", "role": "Overhead", "numChannels": 1, "pan": 1.0 }
      ],
      "channelFaders_dB": { "Top": 0.0, "Bottom": -4.0, "OH_L": -8.0, "OH_R": -8.0, "Room_L": -10.0, "Room_R": -10.0 },
      "channelRouting": { "Top": "bus:Out3", "Bottom": "bus:Out4" }
    }
  ]
}
```

A v1 presetek egyetlen hangszer-szintű MIDI note-ja és velocity-rácsa betöltéskor
automatikusan egy `Main` artikulációvá alakul; a hiányzó Spot és L/R pan adatok is
kompatibilis alapértéket kapnak.

## 6.3 Fájl-hivatkozások

- A mintafájl-útvonalak **relatívak a preset fájl (vagy egy hozzá tartozó
  sample-mappa) helyéhez**, hogy a preset áthelyezhető/megosztható legyen a teljes
  mintakönyvtárral együtt.
- Betöltéskor, ha egy hivatkozott fájl nem található, a plugin **"missing samples"**
  dialógust mutat, ahol a felhasználó egy gombnyomással újra tallózhatja a
  mintakönyvtár gyökérmappáját (automatikus relatív-út feloldás onnantól).

## 6.4 Mentés/betöltés UX

- "Save Preset" / "Save Preset As" / "Load Preset" a globális beállítások panelen.
- Gyors A/B összehasonlításhoz opcionálisan **részleges mentés/betöltés** is
  elérhető (pl. csak a fadereket, vagy csak a MIDI-mapet érintve), de az
  alapértelmezett mentés mindig a teljes 6.1 szerinti állapotot rögzíti.

## 6.5 Hordozható drumset export

A felső sáv `DRUMSET` menüjének `Export Drumset` művelete önállóan szállítható
mappát készít:

```text
Kit_Name_Swarm_Drumset/
├─ preset.json
├─ swarm-drumset.json
└─ Samples/
   └─ 01-kick/
      └─ main/
         └─ Layer-01/
            └─ Slot-001/
```

- A `preset.json` minden sample-hivatkozása a csomag gyökeréhez viszonyított
  relatív útvonal, ezért a teljes mappa változtatás nélkül áthelyezhető.
- A minták hangszer, artikuláció, velocity layer és round-robin slot szerint
  rendezett almappákba kerülnek.
- Hiányzó forrásfájl esetén az export nem ír félkész presetet, hanem felsorolja
  a hiányzó hivatkozásokat.
- Az `Update Drumset Export` csak érvényes `swarm-drumset.json` manifesttel
  rendelkező exportot frissít. Az aktuális presetet és a megváltozott mintákat
  felülírja, az exportmappában található ismeretlen felhasználói fájlokat nem törli.
