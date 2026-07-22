# clients/android

Rohe Demo-App, die den vollen Pfad einmal end-to-end zeigt: verbinden, WS-
Handshake, Video anzeigen, Audio abspielen, Input zurücksenden. Kein Lobby-
Picker (siehe [`docs/protocol.md`](../../docs/protocol.md) dazu, warum die
Lobby dafür ungeeignet ist), keine hübsche Touch-Oberfläche — eine
Host:Port-Eingabe, ein Videobild, zehn Hold-to-press-Buttons.

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

Dieses Projekt wurde in dieser Umgebung **nicht mit Gradle gebaut** — hier
stehen weder Android NDK/Build-Tools noch ein Emulator/Gerät zur Verfügung
(nur `platform-tools`/`adb`, JDK 8 statt der von aktuellem AGP benötigten
JDK 17). Stattdessen wurde der native Teil separat verifiziert: `jni_bridge.c`
+ alle `core/`-Quellen wurden mit einem echten NDK r27c `clang` für
`arm64-v8a` und `x86_64` kompiliert und gelinkt (`-Wall -Wextra`, keine
Warnungen), die drei `Java_com_finlink_android_GbaStreamClient_*`-JNI-Symbole
sind korrekt im resultierenden `.so` exportiert. Die Kotlin-/Gradle-/
Resource-Seite (Compile, `R`-Klassen-Generierung, APK-Packaging) ist
**ungetestet** — zum eigentlichen Bauen und Ausprobieren in Android Studio
öffnen, das lädt NDK/Build-Tools/JDK automatisch nach:

```sh
# In Android Studio: File > Open > clients/android
# oder von der Kommandozeile mit vollem SDK/NDK/JDK17 vorhanden:
cd clients/android
./gradlew assembleDebug
```

## Ausprobieren

1. Dolphin (`dolphin-gba-stream`-Fork) mit einem GC-Port auf „GBA
   (Client-Stream)“ starten.
2. In der App `host:port` des jeweiligen Player-Ports eingeben (z. B.
   `192.168.1.5:6801`) und „Verbinden“.

## Bekannte Lücken (bewusst außerhalb des Rahmens dieser rohen Demo)

- Kein automatischer Player-Picker (manuelle Port-Eingabe statt Lobby-Abfrage
  über alle vier `/status`-Endpunkte)
- Touch-Overlay ist eine einzelne Button-Reihe, kein D-Pad-Layout
- Keine Reconnect-Logik bei Verbindungsabbruch
