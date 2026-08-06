# 5. MIDI kiosztás

## 5.1 Konfigurálhatóság

- Minden hangszernek legalább egy artikulációja van, és **minden artikuláció saját
  trigger MIDI note-ot** kap. A normál, egyetlen hangú hangszerek ezt egy `Main`
  artikulációként használják.
- A Hi-hat legalább `Closed`, `Half Open` és `Open` artikulációt tartalmaz külön
  MIDI note-on. A Crash és China cintányérok `Hit` és `Choke`, a Ride pedig
  `Hit` és `Bell` artikulációval rendelkezik, szintén külön note-okon.
- A hangszer editor fejlécében **"MIDI Learn"** gomb: megnyomva a plugin várja a
  következő beérkező MIDI note-ot, és azt rendeli hozzá a hangszerhez.
- Ütközés-ellenőrzés: ha a kiválasztott note már foglalt egy másik hangszerhez,
  a UI figyelmeztet, és választható, hogy felülírja-e (a másik hangszer note
  nélkül marad, amíg új note-ot nem kap) vagy megszakítja a hozzárendelést.
- Egy globális **MIDI-map táblázat** (hangszer ↔ note, mindkét irányban
  szerkeszthető: kattintás a note mezőre → beírható szám, vagy MIDI Learn) a
  globális beállítások panelen tekinthető át és exportálható/importálható
  presetként.

## 5.2 Alapértelmezett note-tábla — e-kit-stílusú kiosztás (**BECSLÉS**)

> **Fontos:** a felhasználó egy e-dob-kiosztást ábrázoló képet adott meg
> referenciaként (billentyűzet-grafika, note-számok nélkül, csak a
> hangszerek/zónák pozíciója és sorrendje volt leolvasható). A note-számok
> ezért **nem pixelpontosan ellenőrzött, legjobb becslésű** hozzárendelések —
> egyetlen emelkedő note-sorozat (24-től 72-ig) a képen látott bal-jobb
> csoportosítás sorrendjében, nem hitelesített hardveradat. Bármelyik note
> másodpercek alatt javítható hangszerenként a MIDI Learn gombbal (5.1) —
> a note-hozzárendelés a preset egy önálló, szabadon felülírható rétege,
> úgyhogy egy pontatlan alapérték itt nem kockázatos. Ez a tábla csak a
> `DefaultKitFactory::createDefaultKit()`-tel létrehozott friss kitet
> érinti; meglévő mentett presetek nem változnak.

| Hangszer / artikuláció | Note | Note név |
|---|---|---|
| Hi-hat — Pedal CC | 24 | C0 |
| Kick — Kick 2 | 25 | C#0 |
| Kick — Main | 26 | D0 |
| Snare — Hit | 27 | D#0 |
| Snare — Off-center | 28 | E0 |
| Snare — Flam | 29 | F0 |
| Snare — SideStick | 30 | F#0 |
| Snare — No-wires | 31 | G0 |
| Tom 1 | 32 | G#0 |
| Tom 2 | 33 | A0 |
| Tom 3 | 34 | A#0 |
| Tom FX | 35 | B0 |
| Tom 4 | 36 | C1 |
| Hi-hat — Tip Closed | 37 | C#1 |
| Hi-hat — Closed | 38 | D1 |
| Hi-hat — Open 1 | 39 | D#1 |
| Hi-hat — Open 2 | 40 | E1 |
| Hi-hat — Open 3 | 41 | F1 |
| Hi-hat — Open 4 | 42 | F#1 |
| Hi-hat — Open 5 | 43 | G1 |
| Hi-hat — Pedal | 44 | G#1 |
| Hi-hat — Pedal Open | 45 | A1 |
| Hi-hat — Tip Open | 46 | A#1 |
| Crash 1 — Left Hit | 47 | B1 |
| Crash 1 — Right Hit | 48 | C2 |
| Crash 1 — Left Choke | 49 | C#2 |
| Crash 1 — Right Choke | 50 | D2 |
| Crash 2 — Left Hit | 51 | D#2 |
| Crash 2 — Right Hit | 52 | E2 |
| Crash 2 — Left Choke | 53 | F2 |
| Crash 2 — Right Choke | 54 | F#2 |
| Ride — Main | 55 | G2 |
| Ride — Bell | 56 | G#2 |
| Ride — Crash | 57 | A2 |
| China — Left Hit | 58 | A#2 |
| China — Right Hit | 59 | B2 |
| China — Left Choke | 60 | C3 |
| China — Right Choke | 61 | C#3 |
| X-Hat — Closed | 62 | D3 |
| Splash 1 — Hit | 63 | D#3 |
| Splash 1 — Choke | 64 | E3 |
| Splash 2 — Hit | 65 | F3 |
| Splash 2 — Choke | 66 | F#3 |
| X-Hat — Open 1 | 67 | G3 |
| X-Hat — Open 2 | 68 | G#3 |
| X-Hat — Open 3 | 69 | A3 |
| X-Hat — Open 4 | 70 | A#3 |
| X-Hat — Open 5 | 71 | B3 |
| X-Hat — Open 6 | 72 | C4 |
