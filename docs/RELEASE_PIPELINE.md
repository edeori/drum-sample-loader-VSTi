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

## Ellenőrzés (2026-10-19)

- Build: https://github.com/edeori/drum-sample-loader-VSTi/actions/runs/39267111695 — Mac és Windows sikeres, Mac aláírás/notarizáció ellenőrizve, Defender találat nélkül.
- Külön Publish-próba: https://github.com/edeori/drum-sample-loader-VSTi/actions/runs/39267132599 — az artifactok letöltése és a Defender-hash ellenőrzése sikeres. `verify_only=true`, a feltöltési lépés kihagyva. Az éles telepítők ETag és méret alapján változatlanok.

## Build kiválasztása a Publish indításakor

A `build_selection` legördülő alapértéke **Latest successful build**. Ilyenkor a `build_run_id` mezőt hagyd üresen: a workflow az alapértelmezett (`main`) ág legutóbb létrehozott sikeres buildjét választja. PR-/másik ági buildek nem választhatók automatikusan. Régi futás újrafuttatása nem lesz ettől a legújabb build. A kiválasztás a Publish végrehajtásakor történik, a futásazonosító egyszer rögzül minden artifacthoz.

Konkrét vagy tagről készült buildhez válaszd a **Specific build ID** opciót, majd add meg a `build_run_id` értékét. Mindkét mód használható `verify_only=true` mellett is. A kiválasztott build linkje és forráscommitja megjelenik az összesítőben; hiányzó/lejárt artifact vagy hibás Defender-jelentés esetén a Publish leáll.

## Live versioned release — 2026-10-19

Version **0.1.1**, built source `18c3dbb9d93574904820950e50f39078adea5e80`.
[Build](https://github.com/edeori/drum-sample-loader-VSTi/actions/runs/39309138516) and [publication](https://github.com/edeori/drum-sample-loader-VSTi/actions/runs/39310089327) succeeded for macOS and Windows.
The published Mac package version and Windows ProductVersion were verified; the
live EXE SHA-256 matches the clean Defender report. Shop version updated after
publication. See the [shared release record](https://github.com/edeori/vst-release-pipeline/blob/main/docs/releases/2026-10-19-version-rollout.md) for all products and hashes.
