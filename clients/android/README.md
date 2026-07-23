# clients/android

Rohe Demo-App, die den vollen Pfad einmal end-to-end zeigt: Lobby-Suche,
verbinden, WS-Handshake, Video anzeigen, Audio abspielen, Input zurücksenden.
Keine hübsche Touch-Oberfläche für die eigentlichen GBA-Tasten — eine
Host-Eingabe (kein Port nötig), ein nativer P1–P4-Picker, ein Videobild, zehn
Hold-to-press-Buttons.

Native P1–P4-Lobby statt der eingebetteten HTML-Lobby-Seite: Die Ports
6801–6804 sind durch das Protokoll fix (siehe
[`docs/protocol.md`](../../docs/protocol.md)), also reicht die Host-IP.
„Suchen“ pollt `GET /status` auf allen vier Ports (`java.net.HttpURLConnection`,
plain HTTP JSON — bewusst *nicht* über `finlink_core`/den WebSocket-Pfad, da es
kein Teil des Stream-Protokolls ist) und zeigt P1–P4 mit belegten/nicht
erreichbaren Slots ausgegraut, analog zur Web-Lobby aus
`GBAStreamClientPage.h`. Die Lobby selbst (Port 6800) liefert dafür keinen
gebündelten Status — jeder Port muss einzeln gefragt werden.

## Architektur

Alle Protokoll-/Transport-Logik läuft nativ in C (`app/src/main/cpp/`), nicht
in Kotlin:

- **`core/`** (via `add_subdirectory`, siehe `cpp/CMakeLists.txt`) — WS-
  Handshake/Framing, App-Protokoll, Deflate. Unverändert aus dem Core, keine
  Android-spezifische Anpassung nötig.
- **`cpp/jni_bridge.c`** — alles, was Android-spezifisch ist: der rohe POSIX-
  Socket (connect/send/recv), ein Hintergrund-Thread für die Session-Loop
  (analog zum serverseitigen Poll-Loop-Muster aus `GBAStreamHost.cpp`: kurzes
  `poll()`-Timeout, danach prüfen ob Input zum Senden ansteht), und die
  JNI-Callbacks zurück nach Kotlin.
- **Kotlin** (`GbaStreamClient.kt`, `MainActivity.kt`) — reine UI-Schicht:
  `GbaStreamClient` ist nur ein dünner Wrapper um die drei nativen Methoden,
  alle eigentliche Arbeit passiert in `jni_bridge.c`. `MainActivity` rendert
  Video-Frames als `Bitmap.Config.RGB_565` (passt exakt zum Wire-Format, keine
  Konvertierung nötig) und spielt Audio über `AudioTrack` im Streaming-Modus.

Zufallsbytes für WS-Key und Frame-Maskierung kommen von `arc4random_buf`
(Bionic-libc-Standard, kein zusätzlicher Dependency).

## Bauen

Diese Umgebung hatte ursprünglich weder Android NDK/Build-Tools noch JDK 17
noch Gradle (nur `platform-tools`/`adb`, JDK 8). Für einen vollständigen Build
wurden JDK 17 (Temurin), Android cmdline-tools, `platform-tools`,
`platforms;android-34`, `build-tools;34.0.0` und Gradle 8.7 lokal
nachinstalliert (AGP hat sich beim ersten Build zusätzlich selbst noch
NDK 26.1.10909125 nachgeladen, da keine `ndkVersion` in `app/build.gradle.kts`
gepinnt ist). Damit lief `./gradlew assembleDebug` **vollständig durch** —
Kotlin-Compile, CMake/NDK-Build für alle vier Default-ABIs
(arm64-v8a/armeabi-v7a/x86/x86_64), Resource-Packaging, alles erfolgreich,
resultierende `app-debug.apk` mit `aapt dump badging` verifiziert (Package
`com.finlink.android`, `INTERNET`-Permission, `MainActivity` als
Launcher-Activity, alle vier `.so`-ABIs und Icons enthalten). Nicht getestet:
Installation/Ausführung auf einem echten Gerät oder Emulator — hier ist keins
verfügbar (`adb devices` liefert eine leere Liste).

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
2. In der App die Host-IP eingeben (z. B. `192.168.1.5`) und „Suchen“.
3. Einen freien P-Slot antippen.

Auf echter Hardware (Samsung Galaxy S22, per WLAN-`adb`) verifiziert: App
startet ohne Crash, Lobby-Suche läuft (inkl. `usesCleartextTraffic` für die
`HttpURLConnection`-Aufrufe), P1–P4 werden korrekt als belegt/nicht erreichbar
ausgegraut. Verbindung zu einem tatsächlich laufenden Dolphin-Stream wurde
nicht getestet (kein Dolphin-Host im Testnetz verfügbar).

## Bekannte Lücken (bewusst außerhalb des Rahmens dieser rohen Demo)

- Touch-Overlay ist eine einzelne Button-Reihe, kein D-Pad-Layout
- Keine Reconnect-Logik bei Verbindungsabbruch
- Lobby-Suche fragt die vier Ports nacheinander ab, nicht parallel (bei
  Timeouts entsprechend langsamer)
