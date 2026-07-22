# core

Portable C99-Library (CMake) mit der plattformunabhängigen Protokoll-/Codec-/
Transport-Logik, siehe [`docs/protocol.md`](../docs/protocol.md). Reine
Buffer-in/Buffer-out-Logik, kein Socket-I/O — das rohe TCP-Handling (connect,
send/recv) übernimmt jede Plattform selbst in [`../clients/`](../clients/), da
es dort ohnehin nicht vermeidbar plattformspezifisch ist (BSD-Sockets via
libctru/libnx vs. Android-APIs). Alles, was reine Logik über diesen Bytes ist —
inklusive des WebSocket-Handshakes und -Framings selbst — liegt hier, weil
3DS/Switch-Homebrew keinerlei eingebauten WebSocket-Client mitbringen.

- `include/finlink/websocket.h` + `src/websocket.c` — RFC6455-Client-Handshake
  (Key-Erzeugung, Request-Aufbau, Accept-Validierung) und Frame-Reader/-Writer,
  abgestimmt auf das serverseitige Verhalten in `GBAStreamHost.cpp`
  (unmaskierte, nicht fragmentierte Server-Frames; maskierte Client-Frames;
  kein Ping/Pong, kein permessage-deflate). Nutzt vendored `teeny-sha1` (MIT,
  siehe `third_party/teeny-sha1/LICENSE`) für den SHA1 im Handshake.
- `include/finlink/protocol.h` + `src/protocol.c` — (De-)Serialisierung der drei
  Nachrichtentypen (Video-Header, Audio-Frame, Input-Bitmask) innerhalb eines
  WebSocket-Frame-Payloads
- `include/finlink/inflate.h` + `src/inflate.c` — raw-deflate-Inflate der
  Video-Payload, Wrapper um vendored `tinfl` (miniz, MIT, siehe
  `third_party/miniz/LICENSE`)
- `include/finlink/endian.h` — portable Little-Endian-Reads/Writes fürs Wire-Format

RGB565-Konvertierung, PCM-Audio-Buffering und Input-Polling sind bewusst nicht Teil
des Cores — die hängen an Rendering-/Audio-APIs der jeweiligen Plattform. Woher
die für den Handshake und die Frame-Maskierung nötigen Zufallsbytes kommen, ist
ebenfalls Sache des Aufrufers (Hardware-TRNG bis `rand()`) statt im Core
festgelegt — die Plattformen unterscheiden sich hier zu stark, um sich sinnvoll
auf eine Quelle festzulegen.

## Bauen (Host, für Entwicklung/Tests)

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Client-Builds binden diese Library per `add_subdirectory(../../core)` ein statt sie
eigenständig zu konfigurieren (Tests werden dann automatisch übersprungen).
