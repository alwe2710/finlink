# NDS-Client: Machbarkeitsanalyse (Stand 2026-07-23)

Status: **zurückgestellt** — NDS ist ein späteres Nice-to-have, nicht Teil der
ersten Implementierungsrunde (Android, 3DS, Switch zuerst).

## Kernproblem: WLAN-Hardwarelimit

Die NDS-WLAN-Hardware unterstützt nur die 802.11b-Transferraten **1 und 2 Mbit/s**
(kein 5,5/11 Mbit/s). Reales TCP-Throughput über `dswifi` liegt durch Protokoll-
Overhead noch darunter. Das ist eine harte Hardwaregrenze, keine Softwareschwäche.

## Bandbreiten-Budget gegen das aktuelle Protokoll

- **Audio allein**: z. B. 32 kHz, Stereo, 16-bit PCM → 128.000 B/s ≈ **1,02 Mbit/s**.
  Das entspricht bereits der gesamten realistischen WLAN-Kapazität der NDS —
  noch bevor überhaupt Video dazukommt.
- **Video**: 240×160 × RGB565 = 76.800 B/Frame unkomprimiert. Selbst mit
  optimistischer 3–4×-Deflate-Kompression (Kompressionsrate ist inhaltsabhängig,
  keine Garantie bei bewegungsreichen Szenen) bleiben ~19–25 kB/Frame.

**Fazit**: Voller Original-Stream (Stereo-Audio + native Framerate) ist mit dem
aktuellen Wire-Protokoll auf NDS **nicht machbar**.

## Präzedenzfälle

Bekannte NDS-Homebrew-Streaming-Projekte (z. B. `streamer-ds`) lösen das
vergleichbare Problem nur durch drastisch reduzierte Auflösung/Framerate und
LZ77-Kompression — kein Fall von Vollqualitäts-Streaming über NDS-WLAN gefunden.

## Optionen für einen späteren NDS-Client

1. **Kein/kaum Audio** (z. B. 8 kHz mono ≈ 128 kbit/s) + reduzierte Framerate
   (Schätzung: einstellige fps-Bereich, abhängig von tatsächlicher Kompression).
2. **Serverseitige Protokollerweiterung** um Qualitäts-/Framerate-Verhandlung
   (Änderung am `dolphin-gba-stream`-Fork, außerhalb dieses Repos).
3. NDS **nicht** als Live-Stream-Client, sondern reduzierter Anwendungsfall
   (z. B. nur Status-Anzeige/Lobby via `/status`, kein Video/Audio).

Diese Analyse basiert auf dokumentierten Hardware-Grenzwerten, nicht auf einem
Test mit echter Hardware/Emulator (in dieser Umgebung nicht verfügbar — kein
devkitPro-Toolchain, keine NDS-Hardware). Vor einer finalen Entscheidung sollte
das auf echter Hardware verifiziert werden.

## Quellen

- [DSWifi documentation – BlocksDS](https://blocksds.skylyrac.net/dswifi/)
- [Wi-Fi – BlocksDS Tutorial](https://blocksds.skylyrac.net/tutorial/advanced/wifi/)
- [streamer-ds – GameBrew](https://www.gamebrew.org/wiki/Streamer-ds)
