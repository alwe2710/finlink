# finlink

Generisches Client-Framework für den Dolphin GBA-Stream.

## Kontext

Im Fork [dolphin-gba-stream](https://github.com/) (Basis: `dolphin-emu/dolphin`) gibt es ein Feature, das
die integrierte GBA-Emulation (`GBA::Core`, genutzt für GC↔GBA-Link-Cable-Spiele) per eigenem
WebSocket-Protokoll an Browser-Clients streamt, statt Video/Audio/Input lokal zu behalten.

Dieses Repository ist der Ausgangspunkt für ein **generelles Client-Framework** für diesen Stream —
losgelöst von der aktuellen monolithischen, eingebetteten HTML/JS-Client-Implementierung auf
C++-Server-Seite.

### Serverseitige Architektur (C++, bereits vorhanden, nicht Gegenstand dieses Repos)

- **`StreamHost`** (`Source/Core/Core/HW/GBAStreamHost.h/.cpp`): eine Instanz pro GC-Port, der auf
  „GBA (Client-Stream)“ (`CORE_GC_GBA_STREAM`) steht. Läuft auf Port 6801–6804, sendet Video
  (RGB565 + raw-deflate) und Audio (PCM) an genau einen verbundenen Client, empfängt Button-States
  zurück und speist sie via `ControllerEmu::SetInputOverrideFunction` in den GBA-Pad ein.
- **`GBAStreamLobby`** (`GBAStreamLobby.h/.cpp`): referenzgezählter Singleton-Server auf festem
  Port 6800, liefert die Picker-Seite (P1–P4) aus, unabhängig davon welcher GC-Port aktiv ist.
- Gemeinsame Client-Seite liegt aktuell als ein einziger großer eingebetteter HTML/JS-String in
  `GBAStreamClientPage.h` (`kGBAStreamClientHtml`, ein C++ `R"HTML(...)HTML"`-Rohstring), der von
  beiden Servern ausgeliefert wird. Kein Build-Schritt, keine externen JS-Dependencies (bewusst so
  gewählt, um serverseitigen Aufwand minimal zu halten).

### Wire-Protokoll (WebSocket, binär)

| Richtung | Typ | Format |
|---|---|---|
| Server → Client | `1` (Video) | `[u8 type=1][u32le width][u32le height][raw-deflate RGB565-Pixel]` |
| Server → Client | `3` (Audio) | `[u8 type=3][u32le sampleRate][u8 channels][s16le PCM-Samples]` |
| Client → Server | `2` (Input) | `[u8 type=2][u16le keyBitmask]` (Bitreihenfolge: A, B, Select, Start, Right, Left, Up, Down, R, L) |

Zusätzlich: `GET /status` (nur auf Player-Ports 6801–6804) liefert JSON `{"occupied": bool}` mit
CORS, für die Lobby-Belegungsanzeige.

## Ziel dieses Repos

Ein eigenständiges, generelles Client-Framework für dieses Protokoll bauen — als Ablösung/Ergänzung
zum eingebetteten HTML/JS-String auf Serverseite.
