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

## 5.2 Alapértelmezett "Krimh-stílusú" note-tábla — **PLACEHOLDER**

> **Fontos:** a Krimh Drums (Bogren Digital) hivatalos MIDI-kiosztásának pontos
> note-számait nem sikerült megbízható forrásból ellenőrizni (a hivatalos
> kézikönyv szkennelt/képi PDF, szövegesen nem kereshető). Az alábbi tábla ezért
> **General MIDI szabvány + a metál dob-produkciós közösségben elterjedt
> konvenciók** alapján összeállított, **szerkeszthető kiindulási sablon**, nem
> hitelesített Krimh-adat. Ha előkerül a pontos hivatalos lista, ez a táblázat
> egy az egyben lecserélhető anélkül, hogy bármi mást a tervben módosítani
> kellene — a note-hozzárendelés a preset egy önálló, szabadon felülírható
> rétege (lásd 5.1).

| Hangszer | Alapértelmezett note | Note név |
|---|---|---|
| Kick | 36 | C1 |
| Snare | 38 | D1 |
| Hi-hat (closed) | 42 | F#1 |
| Hi-hat (half open) | 44 | G#1 |
| Hi-hat (open) | 46 | A#1 |
| Tom 1 (legmagasabb) | 50 | D2 |
| Tom 2 | 48 | C2 |
| Tom 3 | 45 | A1 |
| Tom 4 (legmélyebb) | 41 | F1 |
| Crash 1 | 49 | C#2 |
| Crash 1 choke | 61 | C#3 |
| Crash 2 | 57 | A2 |
| Crash 2 choke | 58 | A#2 |
| Ride hit | 51 | D#2 |
| Ride bell | 53 | F2 |
| China hit | 52 | E2 |
| China choke | 60 | C3 |
| Splash | 55 | G2 |
| *(további cintányérok)* | 58, 59, 60, ... | szabadon bővíthető |

- Ez a tábla csak a **kezdeti default preset** kiosztása — a felhasználó bármikor
  szabadon átírhatja hangszerenként (5.1), így a tényleges éles Krimh-map (vagy
  bármilyen más DAW/library map, pl. Superior Drummer, EZdrummer, Addictive
  Drums) egyszerűen betallózható MIDI Learn-nel, vagy egy külön "MIDI map preset"
  fájlként importálható, ha a jövőben elkészül egy ilyen exportfájl.
