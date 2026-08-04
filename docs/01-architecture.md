# 1. Architektúra

## 1.1 Alapok

- **JUCE `AudioProcessor`** alapú plugin, VST3 és AU célformátummal.
- A hangmotor **teljes egészében a JUCE audio thread-en** fut; a MIDI-feldolgozás,
  a round-robin/velocity kiválasztás és a voice-indítás mind az audio callback-en
  belül, allokáció- és lock-mentesen történik.
- **Minden minta előre RAM-ba töltve** (nincs disk streaming az első verzióban) —
  ez a legegyszerűbb módja a kiszámítható, alacsony latency-nek. A mintakönyvtár
  betöltése a UI/message threaden történik, háttérszálon, a preset megnyitásakor.

## 1.2 Magas szintű jelfolyam

```mermaid
flowchart LR
    MIDI[MIDI bemenet] --> Router[Note Router\n(hangszer feloldás MIDI note alapján)]
    Router --> VelSel[Velocity-sáv kiválasztás\n(globális threshold-ok)]
    VelSel --> RR[Round-robin választó\n(előző slot kizárva)]
    RR --> Voices[Voice pool\n(egy voice / mikrofoncsatorna / leütés)]
    Voices --> ChGain[Csatorna faderek\n(pl. Top, Bottom, OH L/R, Room L/R)]
    ChGain --> BusSum[Busz-összegzés\nOverhead-busz / Room-busz / Spot-buszok]
    BusSum --> MasterOH[Master OH fader]
    BusSum --> MasterRoom[Master Room fader]
    MasterOH --> Routing[Kimeneti routing mátrix]
    MasterRoom --> Routing
    ChGain --> Routing
    Routing --> Out1[Host kimeneti busz 1]
    Routing --> Out2[Host kimeneti busz 2]
    Routing --> OutN[Host kimeneti busz N]
    Routing --> StereoMix[Belső stereo mixdown\n(ha a host nem multi-out)]
```

## 1.3 Voice pool és polifónia ("let it ring")

- Minden leütéskor **egy voice indul minden érintett mikrofoncsatornára** (pl. egy
  snare heavy hit 4 mikrofoncsatornával = 4 párhuzamos voice, szinkron indítással).
- A voice-ok **teljes hosszban lejátszódnak**, függetlenül az azonos hangszeren
  bekövetkező következő normál leütéstől — nincs automatikus voice-stealing.
- A Crash és China hangszerek külön MIDI note-ról triggerelhető `Choke`
  artikulációja explicit kivétel: rövid lecsengéssel lezárja az adott cintányér
  aktív voice-ait.
- Ebből következően a voice pool méretét **nagyvonalúan, statikusan előre kell
  allokálni** (pl. hangszerenként N egyidejű leütésnyi kapacitás, konfigurálható
  felső korlát), hogy elkerüljük a futásidejű allokációt.
- **Biztonsági tartalék:** ha a pool extrém esetben (pl. MIDI-vihar) kimerülne, a
  legrégebb óta futó, már elhalványuló (near-silent tail) voice kerül lecserélésre,
  rövid (pár ms) crossfade-del a kattanás elkerülésére. Ez kizárólag vészhelyzeti
  védelem, nem befolyásolja a normál "let it ring" viselkedést.

## 1.4 Multi-out busz architektúra

- A plugin JUCE `AudioProcessor::BusesProperties` alapján **több sztereó (vagy mono)
  kimeneti buszt** definiál — hasonlóan a Kontakt/EZdrummer multi-out módjához.
- Alapértelmezett buszlayout javaslat:
  - `Master` (stereo, minden hangszer összegzett mixe)
  - `Kick In`, `Kick Out` (mono)
  - `Snare Top`, `Snare Bottom` (mono)
  - `Tom 1..4` (mono)
  - `Overheads` (stereo)
  - `Room` (stereo)
  - `Cymbals` (opcionális gyűjtőbusz, ha a host korlátozza a buszok számát)
- A tényleges hozzárendelés (melyik hangszer melyik mikrofoncsatornája melyik
  buszra menjen) **teljesen szabadon konfigurálható** a routing mátrixban —
  lásd [04-mixing-routing.md](04-mixing-routing.md).
- Ha a host nem támogatja a multi-out-ot (vagy a felhasználó nem konfigurál
  routingot), minden csatorna a `Master` stereo buszba keveredik a beállított
  faderekkel.

## 1.5 Low-latency elvek (összefoglalva)

1. Minden minta RAM-ban, előre dekódolva (nincs valós idejű fájlformátum-dekódolás).
2. Lock-free, előre allokált voice pool — nincs `new`/`malloc` az audio threaden.
3. MIDI-feldolgozás és voice-indítás ugyanabban az audio blokkban történik, mint
   a bejövő MIDI esemény — nincs köztes üzenetsor-késleltetés.
4. A minta-hozzárendelések, routing és fader-változtatások a UI threaden módosítják
   az állapotot, de a hangmotor csak atomikus/lock-free olvasott másolatot használ
   (pl. `std::atomic`, double-buffered paraméter struktúrák), hogy a UI-szál soha
   ne blokkolja az audio-szálat.
