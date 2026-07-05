# Pokyny pro Claude — PictureViewer_v2

## Vydávání nových verzí (release)

**Release proces vždy provádí Claude celý — nikdy nepředává uživateli seznam kroků k ručnímu spuštění.**

Postup pro vydání nové verze:

1. Zvýšit `VERSION` v [`CMakeLists.txt`](CMakeLists.txt) (řádek 3, `project(PictureViewer VERSION X.Y …)`).
2. Aktualizovat číslo verze v [`README.md`](README.md) (řádek s „Aktuální verze **X.Y**").
3. Commitnout.
4. `git tag vX.Y && git push origin vX.Y`.

Push tagu spustí workflow [`.github/workflows/release.yml`](.github/workflows/release.yml), který:
- ověří, že tag odpovídá `VERSION` v `CMakeLists.txt`,
- sestaví Windows instalátor (Inno Setup, [`installer/setup.iss`](installer/setup.iss)),
- vygeneruje `SHA256SUMS.txt`,
- vytvoří GitHub Release.

Tento release konzumuje in-app aktualizátor (`UpdateChecker`, `src/app/UpdateChecker.*`), takže starší buildy si přes **Nápověda → Zkontrolovat aktualizace…** novou verzi najdou, ověří přes SHA256 a nainstalují.

## Verze — jediný zdroj pravdy

Verze aplikace je definována **pouze** v `project(VERSION …)` v `CMakeLists.txt` a šíří se do kódu přes compile definition `PV_APP_VERSION`. Nikdy ji znovu natvrdo nepiš do `Application.cpp` ani jinam.
