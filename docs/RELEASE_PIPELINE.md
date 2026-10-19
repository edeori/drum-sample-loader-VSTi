# Swarm release pipeline

Repo: `edeori/drum-sample-loader-VSTi`; termékazonosító és plugin neve: `swarm` / `Swarm`.

A saját GitHub Actions felületen:

- **Build installers** (`release.yml`): kézzel vagy `v*` tag push után Mac és Windows telepítőt készít. A Mac csomag aláírt és notarizált; a Windows builden kötelező Defender-vizsgálat fut. Nem publikál.
- **Publish installers** (`publish.yml`): csak kézzel, a `main` ágról indítható. A `build_run_id` mezőbe a sikeres build URL-jének végén lévő szám kerül. Ugyanazokat az artifactokat tölti fel újrafordítás nélkül. `verify_only=true` esetén csak ellenőriz, feltöltés nincs.

A Publish kézi indítása a jóváhagyás, külön environment gate nincs. A közös workflow ellenőrzi a build eredetét, sikerességét és a Windows EXE egyezését a tiszta Defender-jelentéssel. Az artifactok 30 napig érhetők el. Régebbi build kiválasztása korábbi verziót állít vissza. A két MinIO-feltöltés nem atomi; részleges hiba esetén ugyanazt a buildet publikáld újra.

Célok:
- https://download.mothproduction.hu/downloads/plugins/swarm/mac/Swarm.pkg
- https://download.mothproduction.hu/downloads/plugins/swarm/win/Swarm.exe

A Windows-telepítő x64 VST3-at és választható standalone alkalmazást tartalmaz, a Mac csomag AU-t is. A projektben jelenleg nincs aktiválási/licencelési modul. A Windows-telepítő nem Authenticode-aláírt; a Defender-vizsgálat nem biztosít SmartScreen-reputációt.

A közös workflow teljes commitazonosítóval rögzített az `edeori/vst-release-pipeline` repóból. A build nem kapja meg a MinIO feltöltési kulcsait.
