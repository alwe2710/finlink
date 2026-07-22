# core

Portable C99-Library (CMake) mit der plattformunabhängigen Protokoll-/Codec-Logik,
siehe [`docs/protocol.md`](../docs/protocol.md). Reine Buffer-in/Buffer-out-Logik,
keine I/O — die WebSocket-Transportschicht (Handshake, Socket-I/O) ist
plattformspezifisch und lebt in [`../clients/`](../clients/).

- `include/finlink/protocol.h` + `src/protocol.c` — (De-)Serialisierung der drei
  Nachrichtentypen (Video-Header, Audio-Frame, Input-Bitmask)
- `include/finlink/inflate.h` + `src/inflate.c` — raw-deflate-Inflate der
  Video-Payload, Wrapper um vendored `tinfl` (miniz, MIT, siehe
  `third_party/miniz/LICENSE`)
- `include/finlink/endian.h` — portable Little-Endian-Reads/Writes fürs Wire-Format

RGB565-Konvertierung, PCM-Audio-Buffering und Input-Polling sind bewusst nicht Teil
des Cores — die hängen an Rendering-/Audio-APIs der jeweiligen Plattform.

## Bauen (Host, für Entwicklung/Tests)

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Client-Builds binden diese Library per `add_subdirectory(../../core)` ein statt sie
eigenständig zu konfigurieren (Tests werden dann automatisch übersprungen).
