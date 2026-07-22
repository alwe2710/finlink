# Wire-Protokoll: Dolphin GBA-Stream

Single Source of Truth für das WebSocket-Protokoll, gegen das alle Clients in diesem
Repo implementieren. Server-Referenzimplementierung: `GBAStreamHost` /
`GBAStreamLobby` im `dolphin-gba-stream`-Fork (nicht Teil dieses Repos).

## Endpunkte

| Server | Port | Zweck |
|---|---|---|
| `GBAStreamLobby` | `6800` | Picker-Seite, referenzgezählter Singleton, unabhängig vom aktiven GC-Port |
| `StreamHost` (× GC-Port) | `6801`–`6804` | Ein Slot pro GC-Port, der auf „GBA (Client-Stream)“ steht. Genau ein verbundener Client pro Port. |

## WebSocket, binäre Frames

| Richtung | Typ | Format |
|---|---|---|
| Server → Client | `1` (Video) | `[u8 type=1][u32le width][u32le height][raw-deflate-komprimierte RGB565-Pixel]` |
| Server → Client | `3` (Audio) | `[u8 type=3][u32le sampleRate][u8 channels][s16le PCM-Samples]` |
| Client → Server | `2` (Input) | `[u8 type=2][u16le keyBitmask]` |

Bitreihenfolge Input-Bitmask (Bit 0 = LSB): `A, B, Select, Start, Right, Left, Up, Down, R, L`

Alle Mehrbyte-Felder sind Little-Endian.

## WebSocket-Handshake und -Framing

Serverseitig ist das WebSocket-Handling selbst (nicht nur das App-Layer-Protokoll
oben) handgerollt (`GBAStreamHost::PerformHandshake`, `TryParseWebSocketFrame`,
`SendWebSocketBinaryFrame`), nicht das Standardverhalten einer WS-Library. Für
Clients relevant, insbesondere auf Plattformen ohne eigenen WS-Client
(3DS/Switch-Homebrew):

- Handshake ist Standard-RFC6455: `Sec-WebSocket-Key` → `SHA1(key + "258EAFA5-
  E914-47DA-95CA-C5AB0DC85B11")` → Base64 → `Sec-WebSocket-Accept`, vom Client
  zu verifizieren.
- Server-Frames sind immer unmaskiert, `FIN=1`, Opcode `0x2` (Binary), 7/16/64-Bit
  Längenfeld je nach Payload-Größe.
- Client-Frames müssen laut RFC **maskiert** gesendet werden.
- **Keine Fragmentierung** (`FIN=0` gilt als Protokollfehler, wird vom Server
  weder gesendet noch akzeptiert), **kein Ping/Pong**, **kein
  `permessage-deflate`** — die Deflate-Kompression passiert ausschließlich
  manuell auf dem Video-Payload (siehe oben), nicht auf WS-Ebene.
- Server schickt beim Schließen keinen Close-Frame zurück; nach Senden/Empfangen
  eines Close-Frames (`Opcode 0x8`) einfach die TCP-Verbindung schließen.

Client-seitige Implementierung dieses Teils liegt in
[`../core/include/finlink/websocket.h`](../core/include/finlink/websocket.h).

## Frame-Semantik (Video-Dedup)

Der Server überspringt Video-Frames, die pixelgleich zum zuletzt gesendeten
Frame sind. Ausbleiben einer neuen Video-Message ist daher normal, kein
Timeout-/Fehlerzustand — Clients müssen einfach das zuletzt empfangene Bild
weiter anzeigen.

## HTTP

`GET /status` — nur auf Player-Ports (6801–6804), nicht auf der Lobby.

```json
{ "occupied": true }
```

Response hat CORS-Header gesetzt (dient der Lobby-Belegungsanzeige). Die Lobby
(Port 6800) liefert auf jedem Pfad unbedingt dieselbe HTML-Seite aus — es gibt
dort **keinen** gebündelten Status über alle vier Player-Ports. Ein eigener
Picker (statt der eingebetteten HTML-Lobby) muss `/status` selbst einzeln auf
6801–6804 pollen.

## Bekannte Einschränkungen / offene Fragen

- Sample-Rate und Kanalzahl des Audio-Streams sind serverseitig konfigurierbar
  (im Frame-Header übertragen), nicht fix — Clients müssen sie pro Stream auslesen,
  nicht hart annehmen.
- Video-Auflösung entspricht dem GBA-Screen (240×160), wird aber ebenfalls im
  Frame-Header übertragen statt hart angenommen.
- Kein eingebauter Mechanismus für Qualitäts-/Framerate-Verhandlung durch den
  Client — der Server sendet, was der emulierte Kern produziert. Relevant für
  bandbreitenschwache Targets, siehe [`nds-feasibility.md`](./nds-feasibility.md).
