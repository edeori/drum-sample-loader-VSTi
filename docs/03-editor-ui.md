# 3. Editor felület

## 3.1 Felület felépítése

- **Kit áttekintő nézet:** a hangszerek listája/rácsa (default kit + hozzáadott extrák),
  mindegyik egy kártyán a nevével, MIDI note-jával és egy gyors mute/solo gombbal.
  Kattintásra megnyílik az adott hangszer részletes editora.
- **Interaktív kit nézet:** a Ride külön kattintható Hit és Bell területet kap;
  a Hi-hat fő területe alapértelmezetten az Open artikulációt játssza le.
- **Hangszer editor panel** (a kártyára kattintva nyílik meg, Kontakt-instrument-editor
  jellegű felugró/beépített panel):
  - Fejléc: hangszernév (átnevezhető), kategória, MIDI note (lásd 05. dokumentum).
  - Mikrofoncsatorna-sorok (pl. Snare-nél: Top, Bottom, OH L/R, Room L/R).
  - Velocity-sávonkénti round-robin minta-rács (lásd 3.3).
  - Csatorna faderek + routing legördülő (lásd [04-mixing-routing.md](04-mixing-routing.md)).
  - Artikulációválasztó; a Hi-hat nyitottsági állapotai és a Crash choke külön
    MIDI note-tal, külön mintakészlettel szerkeszthetők.
  - Audition/preview gomb minden slot-hoz (lejátssza az adott mikrofoncsatorna
    adott slot-jának mintáját, hangszerválasztás nélkül is tesztelhető legyen).
  - A hangszer fejlécének Preview gombja a kijelölt velocity layer tartományának
    közepéről küld note-ont, így a Ghost/Soft/Medium/Heavy rétegek külön hallgathatók.
- **Globális beállítások panel:** velocity threshold konfiguráció (lásd 3.2),
  master faderek, MIDI-map táblázat, valamint külön Overhead és Room csatornaformátum
  (`L/R mono pair` vagy `single stereo channel`).

## 3.2 Globális velocity threshold beállítás

Ez **egyetlen, közös konfiguráció**, ami minden hangszerre egyformán vonatkozik —
nem hangszerenként állítjuk be.

- Felhasználó megadja: **hány velocity-sávra** akarja osztani a 0–127 tartományt
  (pl. 3 sáv: Soft / Medium / Heavy).
- Ehhez **N−1 darab threshold csúszka/számmező** jelenik meg (pl. 2 threshold 3
  sávhoz), amivel a határokat lehet tolni. A UI vizuálisan sávokra osztott
  velocity-sávot mutat (mint egy horizontális sáv-diagram), a csúszkák a
  határvonalak.
- A módosítás azonnal **minden hangszer** minta-rácsát újraosztja az új sávok
  szerint (a már felvett minták a hozzájuk legközelebbi/megfelelő sávban maradnak,
  ha egy sáv megszűnik, a hozzá tartozó minták egy "orphaned samples" listába
  kerülnek, hogy ne vesszenek el véletlenül — a felhasználó dönt az újra-besorolásról).
- Alapértelmezett példa: 4 sáv — *Ghost*, *Soft*, *Medium*, *Heavy* (0–31, 32–63,
  64–95, 96–127), szabadon módosítható.

## 3.3 Minta-hozzárendelés (round-robin rács)

Egy hangszer egy velocity-sávjához tartozó szerkesztőfelület egy **rács**:

```
                 Slot 1        Slot 2        Slot 3        Slot 4
Top (spot)     [ hit1_top ]  [ hit2_top ]  [ hit3_top ]  [ hit4_top ]
Bottom (spot)  [hit1_bot  ]  [hit2_bot  ]  [hit3_bot  ]  [hit4_bot  ]
OH L           [ hit1_ohl ]  [ hit2_ohl ]  [ hit3_ohl ]  [ hit4_ohl ]
OH R           [ hit1_ohr ]  [ hit2_ohr ]  [ hit3_ohr ]  [ hit4_ohr ]
Room L         [hit1_rml  ]  [hit2_rml  ]  [hit3_rml  ]  [hit4_rml  ]
Room R         [hit1_rmr  ]  [hit2_rmr  ]  [hit3_rmr  ]  [hit4_rmr  ]
```

- Minden **oszlop egy round-robin slot** = egy valós, egyidejűleg felvett leütés
  az összes mikrofonon.
- Minden cella egy fájlválasztó: gombra kattintva **mappa-böngésző** nyílik meg
  (natív OS fájlválasztó dialógus), ahonnan a felhasználó a saját, előre elkészített
  dobmintáit tallózhatja be (WAV/AIFF).
- Slot hozzáadása/eltávolítása szabadon (nem kötött 4-hez — a felhasználó annyi
  round-robin mintát vesz fel, amennyit akar, sávonként eltérő számban is).
- Egy adott mikrofoncsatorna cellája **üresen is hagyható** egy slot-on belül
  (pl. ha az adott leütéshez nem volt room-felvétel) — lejátszáskor az üres
  csatorna egyszerűen nem szólal meg, a többi csatorna igen.

## 3.4 Round-robin kiválasztási algoritmus

Leütéskor (adott hangszer, adott velocity-sáv):

1. Ha a sávban **0 slot** van → nincs lejátszás (néma).
2. Ha **1 slot** van → mindig az az egy slot szólal meg (nincs mit véletlenszerűsíteni).
3. Ha **≥2 slot** van:
   - Véletlen index generálása a slot-ok között, **kizárva** az adott hangszer/sáv
     utoljára lejátszott slot-indexét (`lastPlayedSlotPerLayer`).
   - A kiválasztott slot minden kitöltött mikrofoncsatornájának mintája egyszerre,
     szinkron indítással lejátszásra kerül (egy voice / csatorna).
   - `lastPlayedSlotPerLayer` frissül az új index-re.
4. A "nem ismétlődhet" szabály **hangszerenként és velocity-sávonként külön**
   követett állapot — pl. a Snare Heavy sávjának előzménye nem befolyásolja a
   Snare Medium sávját.

## 3.5 "Let it ring" lejátszási modell

- Minden leütés **saját, teljesen független voice-csoportot** indít.
- Egy hangszer egymást követő leütései **soha nem szakítják meg** egymást —
  a korábbi leütés hangja a mintafájl végéig lejátszódik, amíg az új leütés hangja
  is elindul és fut, egymásra rétegződve (természetes drum-kit viselkedés, ahogy
  egy valódi mikrofonozott felvételen is összeadódnak az egymást követő leütések).
- Ez a szabály **minden mikrofoncsatornára** vonatkozik, beleértve az overhead/room
  csatornákat is — ezek sem vágják el egymást hangszerek/leütések között.
- A Crash hangszerek külön MIDI note-on elérhető `Choke` artikulációja tudatos
  kivétel: ez rövid lecsengéssel megállítja az adott Crash aktív voice-ait. Más
  normál leütések továbbra sem vágják el egymást.
