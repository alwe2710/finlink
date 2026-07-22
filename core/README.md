# core

Portable C-Library mit der plattformunabhängigen Protokoll-/Codec-Logik, siehe
[`docs/protocol.md`](../docs/protocol.md):

- WebSocket-Frame-Parsing (Client-Seite)
- raw-deflate-Inflate der Video-Payload
- RGB565 → Zielformat-Konvertierung
- PCM-Audio-Buffering
- Input-Bitmask-Encoding

Wird von allen Plattform-Clients unter [`../clients/`](../clients/) eingebunden.

Noch nicht implementiert.
