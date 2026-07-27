# Changelog

Všechny podstatné změny v projektu **MeteoPlaneRadar**.
Formát vychází z [Keep a Changelog](https://keepachangelog.com/cs/1.1.0/),
verzování je [semantické](https://semver.org/lang/cs/).

Verze je v jediném místě: `src/Version.h` (`FW_VERSION`). Zobrazuje se na
obrazovce Nastavení, na OTA obrazovce a v sériovém výpisu při startu.

---

## [0.4]

> ### ⚠️ Upozornění k aktualizaci na 0.4
> Verze 0.4 mění **rozdělení paměti** (dvě aplikační oblasti, aby bylo kam
> nahrát bezdrátovou aktualizaci). Proto se na ni **z verze 0.3 a nižší nedá
> přejít přes OTA** — je nutné jednou nahrát soubor `*.merged.bin` přes
> [esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz) a USB kabel.
> Od 0.4 dál už aktualizace probíhá bezdrátově.

### Přidáno
- **OTA aktualizace firmware přes WiFi** (ElegantOTA). V Nastavení přibylo
  tlačítko „Firmware update": deska vytvoří AP `MeteoPlaneRadar`, na displeji
  ukáže QR kód a firmware se nahraje z prohlížeče na `192.168.4.1/update`.
  Vyžaduje OTA rozdělení flash (`src/partitions.csv`, dva app sloty).
- **Zapamatování stavu UI** — poslední rozsah (zvlášť pro letadla a meteoradar)
  a naposledy zobrazená obrazovka se ukládají do NVS a obnoví se po restartu.
  Zápis je odložený (~2 s po poslední změně), aby swipování nezatěžovalo flash.
- **Zobrazení verze firmwaru** na obrazovce Nastavení (pod titulkem), na OTA
  obrazovce a v sériovém výpisu. Nová sdílená hlavička `src/Version.h`.
- **Sjednocení nastavení** do `src/Config.h` — časová zóna, výchozí poloha,
  rozsahy, intervaly stahování, název AP a limity na jednom místě.
- **CI build na GitHubu** — každý push se automaticky zkusí přeložit.
- Tento `CHANGELOG.md`, `.gitignore`, `sketch.yaml` a `LICENSE` v kořeni.

### Změněno
- Během OTA se na displeji ukáže jen „Probiha aktualizace…" a **podsvícení se
  vypne** po dobu zápisu. Průběh v procentech se nevykresluje: RGB panel čte
  obraz z PSRAM průběžně a zápis do flash mu data odřezává, takže by obraz
  poskakoval. Procenta jsou vidět v prohlížeči.
- Historie verzí se přesunula z hlavičky `.ino` sem.

### Opraveno
- Meteoradar se při každém vstupu na obrazovku zbytečně znovu dekódoval
  (všech 6 PNG). Nově se přepočítá jen při skutečné změně rozsahu.

## [0.3]

### Změněno
- **Robustní stahování ADS-B.** Celé HTTP tělo se načte do znovupoužitelného
  PSRAM bufferu a parsuje se až kompletní (kontrola utnutí proti
  `Content-Length` + jeden retry), místo parsování přímo z TLS streamu. Tím
  zmizely občasné chyby stahování „IncompleteInput".
- Parsování používá **ArduinoJson filtr** (nechá jen pole, která se používají),
  takže dokument zůstává malý bez ohledu na objem dat.
- Pozemní letadla se zahazují už při parsování.
- **Perioda stahování podle rozsahu** (5 / 10 / 15 s) a po neúspěšném stažení
  dvojnásobek, aby se šetřilo bezplatné API adsb.fi.
- Limit letadel `ADSB_MAX` zvýšen ze 40 na **100**.
- **Ovládání:** dlouhý stisk přepíná obrazovky směrově (levá půlka =
  předchozí, pravá = následující, s přetočením dokola) místo slepého cyklení.

### Opraveno
- Při chybě stahování zůstane poslední platný snímek — radar už nebliká na
  prázdno.

## [0.2]

### Přidáno
- První veřejná verze: radar letadel (adsb.fi) + animovaný srážkový meteoradar
  ČHMÚ na kulatém displeji 480×480.
- Oprava problikávání pixelů uprostřed displeje: jedno plátno v PSRAM a jediný
  přenos snímku synchronizovaný s VSYNC (`num_fbs=1` + bounce buffery,
  pixel clock 8 MHz).
