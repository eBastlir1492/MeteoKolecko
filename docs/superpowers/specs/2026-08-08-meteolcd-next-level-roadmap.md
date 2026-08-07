# MeteoLCD — architektonická vize a Next Level roadmapa

**Datum:** 2026-08-08
**Stav:** Živý backlog po dokončení Level 1
**Vazba:** [Schválená Level 1 specifikace](./2026-08-08-meteolcd-weather-only-design.md)

## 1. Pravidla backlogu

Tento dokument uchovává původní uživatelské nápady i následné architektonické návrhy, aby nezmizely po přerušení chatu nebo dokončení první implementace. Není automatickým oprávněním položky implementovat.

Každá položka má:

- **Původ:** `Uživatel`, `Společný návrh` nebo `Technický návrh`;
- **Stav:** `Nápad`, `Čeká na Level 1 měření`, `Připraveno k brainstormingu`, `Schváleno`, `Hotovo`;
- **Závislosti:** co musí být stabilní před zahájením;
- **Přijetí:** jak poznat, že výsledek přináší hodnotu.

Před implementací každé větší položky se znovu použije brainstorming a vznikne samostatná specifikace a plán.

## NL-01 Automatický jas podle času a polohy

- **Původ:** Uživatel.
- **Stav:** Nápad.
- **Cíl:** Příjemný denní, soumrakový a noční jas bez ručního přepínání.
- **Závislosti:** Stabilní Level 1 settings model, platná poloha, NTP nebo ověřená RTC, známé lokální časové pásmo.
- **Návrh:** Výpočet výšky Slunce z UTC a souřadnic; plynulá interpolace mezi uživatelským denním/nočním limitem; ruční override s dobou platnosti; hysteréze proti blikání.
- **Bezpečný fallback:** Neplatný čas nebo poloha použije uložený ruční jas. Automatika nikdy sama nenastaví nulu.
- **Přijetí:** Bez viditelného skoku jasu, správně kolem DST/soumraku, ruční override je okamžitý a po restartu předvídatelný.

## NL-02 Bohatší víceúrovňová mapa ČR a okolí

- **Původ:** Uživatel.
- **Stav:** Připraveno k brainstormingu po stabilizaci Level 1 rendereru.
- **Cíl:** Hezká orientační mapa včetně relevantního okolí bez potlačení radarové vrstvy.
- **Závislosti:** `Viewport`, offline asset generator, golden renderer a měřený flash/PSRAM budget.
- **Kandidátní vrstvy:** Přesnější státní hranice, okolní státy, kraje, významné řeky, jezera, vodní plochy, hlavní sídla a volitelný jemný terénní stín.
- **Zásada:** Silnice a detailní topografie nejsou výchozí; radar je hlavní informace.
- **LOD:** Samostatné zjednodušení pro 25, 50, 100 a 200 km; žádné runtime parsování GeoJSON.
- **Přijetí:** 1:1 vizuální review všech rozsahů, radar zůstává čitelný, asset má reprodukovatelný původ a měřitelný rozpočet.

## NL-03 Pokročilá typografie a názvy měst

- **Původ:** Uživatel + společný návrh.
- **Stav:** Nápad; Level 1 zavede základní collision-aware label layout.
- **Cíl:** Odstranit kryptické zkratky a nabídnout přirozené české názvy.
- **Závislosti:** Stabilní label layout a mapový generátor.
- **Návrh:** Omezený font s potřebnými českými glyphy; jména s diakritikou; priority podle významu, zoomu, hustoty a vzdálenosti od domova; stabilní kandidátní pozice; volba hustoty; významná přeshraniční města.
- **Přijetí:** Žádné kolize v referenčních pohledech, názvy neblikají mezi animačními rámci, diakritika je čitelná a flash nárůst je zdokumentovaný.

## NL-04 UI design review a vizuální systém

- **Původ:** Uživatel.
- **Stav:** Level 1 obsahuje první review; plný systém je Next Level.
- **Cíl:** Konzistentní radar, menu, servisní stavy a portál.
- **Závislosti:** Screenshot/golden harness a skutečné fotografie LCD.
- **Review sada:** 480×480 náhled bez overlaye, s overlayem, settings, první spuštění, loading, stale, offline, no-data, OTA, den a noc; mobilní portal na úzké i široké obrazovce.
- **Přijetí:** Touch target ≥48 px, čitelný kontrast, žádný důležitý obsah v kruhových rozích, konzistentní barvy/stavy a schválené náhledy před kódováním větší vizuální změny.

## NL-05 Plynulé prolínání radarových snímků

- **Původ:** Společný návrh.
- **Stav:** Čeká na Level 1 měření.
- **Cíl:** Plynulejší vnímání pohybu srážek bez falešné meteorologické predikce.
- **Závislosti:** Přímý RGB565 renderer, stabilní frame timing a paměťové metriky.
- **Návrh:** Fixed-point RGB565 blend sousedních skutečných snímků; automatické vypnutí při překročení render budgetu. Alternativou je paletová radarová vrstva.
- **Vyloučeno:** Optical flow nebo dopočítávání pohybu na ESP32-S3 bez samostatné studie.
- **Přijetí:** Stabilní VSYNC, žádný PSRAM tlak, barvy odpovídají referenci a blend lze vypnout.

## NL-06 Rozšířená FreeRTOS pipeline

- **Původ:** Technický návrh.
- **Stav:** Čeká na Level 1 měření.
- **Cíl:** Odstranit prokazatelné blokování UI, nikoli přidat souběh pro jeho vlastní existenci.
- **Závislosti:** Jednoznačné buffer ownership, diagnostics, failure tests a změřená latence.
- **První stupeň:** Jedna I/O úloha vlastní HTTP a compressed staging; hlavní úloha vlastní displej, touch, decode a render.
- **Možné pokračování:** Samostatný decode worker pouze tehdy, pokud měření ukáže přínos bez zhoršení PSRAM/DMA contention.
- **Komunikace:** Pevné fronty, malé hodnotové zprávy, explicitní cancel/stop handshake pro OTA, žádné sdílené mutable framebuffery.
- **Přijetí:** p95 touch latency se zlepší, VSYNC miss rate se nezhorší, race/failure testy procházejí a task stack high-water marks mají rezervu.

## NL-07 Spolehlivý čas, NTP a RTC

- **Původ:** Technický návrh vyvolaný automatickým jasem a TLS.
- **Stav:** Nápad.
- **Cíl:** Jedna autorita pro UTC, lokální čas, platnost času a drift.
- **Závislosti:** ConnectivityManager a settings timezone.
- **Návrh:** NTP po připojení; ověření RTC na desce; `TimeService` vrací UTC i `isValid`; timestamp radarového snímku zůstává odvozen z filename, nikoli z `now`.
- **Přijetí:** DST testy, boot bez sítě, návrat sítě, neplatná RTC a drift jsou deterministické.

## NL-08 Ověřované HTTPS

- **Původ:** Technický návrh.
- **Stav:** Čeká na spolehlivý čas.
- **Cíl:** Nahradit `setInsecure()` a HTTP GeoIP bez rozbití bootu offline zařízení.
- **Závislosti:** `TimeService`, audit aktuálních endpointů a velikosti CA řetězce.
- **Návrh:** CA certificate/bundle s dokumentovanou obnovou; explicitní error stav při neplatném certifikátu; žádný tichý downgrade.
- **Přijetí:** Platný endpoint projde, expirovaný/MITM certifikát selže, offline režim zachová live radarovou cache.

## NL-09 Více uložených lokalit

- **Původ:** Technický návrh.
- **Stav:** Nápad.
- **Cíl:** Rychlé přepnutí domov/chata/práce bez opakovaného zadávání souřadnic.
- **Závislosti:** Versioned settings, portal forms a viewport.
- **Přijetí:** Pojmenované profily, validní souřadnice, bezpečná migrace a rychlé přepnutí na LCD bez textového vstupu.

## NL-10 Export/import konfigurace

- **Původ:** Společný návrh.
- **Stav:** Nápad.
- **Cíl:** Záloha nastavení a snadná obnova po full-flash aktualizaci.
- **Závislosti:** Stabilní schema a portal.
- **Bezpečnost:** Wi-Fi heslo není ve výchozím exportu; citlivý export vyžaduje samostatné rozhodnutí.
- **Přijetí:** Verze formátu, validace, odmítnutí neznámých hodnot a round-trip test bez ztráty podporovaných polí.

## NL-11 Další meteorologické vrstvy

- **Původ:** Technický návrh.
- **Stav:** Nápad.
- **Cíl:** Rozšířit informační hodnotu až po stabilním meteoradaru.
- **Kandidáti:** Výstrahy, teplota, blesky nebo krátkodobá předpověď.
- **Podmínka:** Každý zdroj vyžaduje licenční, síťový, paměťový a UI audit; nic se nepřidá jen proto, že endpoint existuje.
- **Přijetí:** Vrstva je vypínatelná, nekoliduje s radarem a při selhání neovlivní core weather pipeline.

## NL-12 Aktualizace core a knihoven

- **Původ:** Technický návrh.
- **Stav:** Čeká na dokončené Level 1 regression testy.
- **Cíl:** Bezpečně opustit staré piny bez smíchání kompatibility s feature změnami.
- **Závislosti:** Build matrix, hardware harness, VSYNC/OTA/NVS testy.
- **Postup:** Jedna závislost nebo platforma na commit; přesný changelog; build + hardware smoke + soak podle rizika.
- **Přijetí:** Žádná změna LCD timing, tearing, touch filtru, OTA layoutu ani paměťové stability.

## Parkoviště nápadů

Nové nápady se zapisují jako další `NL-xx` položka, ne pouze do chatu nebo commit message. Při zahájení položky se její stav změní a přidá se odkaz na vlastní schválenou specifikaci a implementační plán.
