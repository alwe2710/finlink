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

## HTTP

`GET /status` — nur auf Player-Ports (6801–6804), nicht auf der Lobby.

```json
{ "occupied": true }
```

Response hat CORS-Header gesetzt (dient der Lobby-Belegungsanzeige).

## Bekannte Einschränkungen / offene Fragen

- Sample-Rate und Kanalzahl des Audio-Streams sind serverseitig konfigurierbar
  (im Frame-Header übertragen), nicht fix — Clients müssen sie pro Stream auslesen,
  nicht hart annehmen.
- Video-Auflösung entspricht dem GBA-Screen (240×160), wird aber ebenfalls im
  Frame-Header übertragen statt hart angenommen.
- Kein eingebauter Mechanismus für Qualitäts-/Framerate-Verhandlung durch den
  Client — der Server sendet, was der emulierte Kern produziert. Relevant für
  bandbreitenschwache Targets, siehe [`nds-feasibility.md`](./nds-feasibility.md).
