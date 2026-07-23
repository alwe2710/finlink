# clients/android

Three-page demo app: **Menu** (host entry + network discovery + link to
Settings), **Settings** (on-screen-controls toggle + physical key bindings),
**Player** (the actual stream: fullscreen video, audio, input). UI is
[Jetpack Compose](https://developer.android.com/compose) with
[Material 3](https://m3.material.io/) — see [Design](#design) below for why
and what that cost in this environment.

## Architektur

Alle Protokoll-/Transport-Logik läuft nativ in C (`app/src/main/cpp/`), nicht
in Kotlin — die drei Activities sind reine UI- und Orchestrierungs-Schicht:

- **`core/`** (via `add_subdirectory`, siehe `cpp/CMakeLists.txt`) — WS-
  Handshake/Framing, App-Protokoll, Deflate. Unverändert aus dem Core, keine
  Android-spezifische Anpassung nötig.
- **`cpp/jni_bridge.c`** — alles, was Android-spezifisch ist: der rohe POSIX-
  Socket (connect/send/recv), ein Hintergrund-Thread für die Session-Loop
  (analog zum serverseitigen Poll-Loop-Muster aus `GBAStreamHost.cpp`: kurzes
  `poll()`-Timeout, danach prüfen ob Input zum Senden ansteht), und die
  JNI-Callbacks zurück nach Kotlin.
- **`GbaStreamClient.kt`** — dünner Wrapper um die drei nativen Methoden,
  alle eigentliche Arbeit passiert in `jni_bridge.c`.
- **`MenuActivity.kt`** — Host-Eingabe + P1–P4-Picker (`GET /status` auf
  6801–6804, plain `HttpURLConnection`, bewusst *nicht* über `finlink_core`/
  den WebSocket-Pfad, da es kein Teil des Stream-Protokolls ist) sowie
  Netzwerk-Discovery (Subnetz-Sweep gegen Port 6800 — Dolphin bewirbt sich
  nicht selbst, kein mDNS/UPnP, siehe [`docs/protocol.md`](../../docs/protocol.md)).
  Die Lobby (6800) liefert dafür keinen gebündelten Status; jeder Player-Port
  muss einzeln gefragt werden.
- **`SettingsActivity.kt`** — On-Screen-Controls-Toggle und
  Tastenzuweisungen (physische Taste/Controller-Taste pro GBA-Button, per
  `dispatchKeyEvent`-Interception), beides in `Prefs.kt` (`SharedPreferences`)
  persistiert.
- **`PlayerActivity.kt`** — verbindet sofort beim Start (Host/Port kommen als
  Intent-Extras von `MenuActivity`), rendert Video als `Bitmap.RGB_565`
  (passt exakt zum Wire-Format) über `Image(contentScale = ContentScale.Fit)`
  (skaliert auf Vollbild, nie gestreckt), spielt Audio über `AudioTrack`,
  kombiniert Touch- und physische Tasteneingaben (getrennte Bitmasken, ODER-
  verknüpft beim Senden — analog zum ursprünglichen Web-Client, der Tastatur/
  Touch/Gamepad genauso mischt).

Zufallsbytes für WS-Key und Frame-Maskierung kommen von `arc4random_buf`
(Bionic-libc-Standard, kein zusätzlicher Dependency).

Menu und Settings haben **keine** Orientierungssperre (folgen der
Geräterotation), Player bleibt landscape-gesperrt (feste GBA-Seitenverhältnis-
Anforderung) — siehe `AndroidManifest.xml`.

## Design

„Modernes Material Design“ hieß hier: UI-Layer komplett von XML-Views auf
Jetpack Compose + Material 3 umgestellt (`Theme.kt`), mit dynamischem Farb-
schema (Material You, Android 12+) und statischem Cyan-auf-Navy-Fallback
passend zu [`assets/logo/`](../../assets/logo/) für ältere Geräte.

Dependency-Versionen sind bewusst *nicht* die aktuellsten: `compose-bom
2024.09.00` + `activity-compose 1.9.2` statt der neuesten Releases, weil
neuere Versionen `compileSdk 35/36` und AGP 8.6+/8.9+ voraussetzen — das hätte
eine Kaskade an weiteren Toolchain-Upgrades in dieser Umgebung ausgelöst.
Funktional vollwertiges Material 3, nur ein bis zwei Jahre hinter der
Speerspitze.

## Bauen

Diese Umgebung hatte ursprünglich weder Android NDK/Build-Tools noch JDK 17
noch Gradle (nur `platform-tools`/`adb`, JDK 8). Für einen vollständigen Build
wurden JDK 17 (Temurin), Android cmdline-tools, `platform-tools`,
`platforms;android-34`, `build-tools;34.0.0` und Gradle 8.7 lokal
nachinstalliert (AGP hat sich beim ersten Build zusätzlich selbst noch eine
NDK-Version nachgeladen, da keine `ndkVersion` in `app/build.gradle.kts`
gepinnt ist).

```sh
cd clients/android
./gradlew assembleDebug
# APK liegt danach unter app/build/outputs/apk/debug/app-debug.apk
```

Voraussetzung: Android SDK mit `platforms;android-34` und
`build-tools;34.0.0` sowie ein `sdk.dir` in `clients/android/local.properties`
(oder `ANDROID_HOME`/`ANDROID_SDK_ROOT` gesetzt) — Android Studio richtet das
beim Öffnen des Projekts automatisch ein.

## Ausprobieren

1. Dolphin (`dolphin-gba-stream`-Fork) mit einem GC-Port auf „GBA
   (Client-Stream)“ starten.
2. In der App die Host-IP eingeben (z. B. `192.168.1.5`) und „Suchen“ — oder
   „Server suchen“ für die automatische Netzwerk-Suche.
3. Einen freien P-Slot antippen.

**Auf echter Hardware verifiziert** (Samsung Galaxy S22, per WLAN-`adb`):
Menu-, Settings- und Player-Screen laufen fehlerfrei in Compose/Material 3,
inkl. dynamischem Farbschema und korrektem Verhalten bei Rotation (Menu/
Settings folgen der Geräteausrichtung, Player bleibt Landscape). Eine echte
Verbindung zu einem laufenden Dolphin-Stream wurde ebenfalls erfolgreich
getestet — Video- und Audio-Wiedergabe funktionieren im Player.

## Bekannte Lücken (bewusst außerhalb des Rahmens dieser Demo)

- Touch-Overlay ist eine einzelne Button-Reihe, kein D-Pad-Layout
- Keine Reconnect-Logik bei Verbindungsabbruch
- Lobby-Suche fragt die vier Ports nacheinander ab, nicht parallel (bei
  Timeouts entsprechend langsamer); die Discovery-Suche läuft dagegen bereits
  parallel (Thread-Pool)
