# MeteoLCD Weather-Only — schválená návrhová specifikace

**Datum:** 2026-08-08
**Stav:** Schváleno pro vytvoření implementačního plánu
**Rozsah:** Úroveň 1 — bezpečná přeměna MeteoPlaneRadar na meteorologický displej

## 1. Účel a zdroje pravdy

Cílem je odstranit leteckou část, zachovat ověřenou stabilitu Waveshare ESP32-S3-Touch-LCD-2.1 a vytvořit testovatelný meteorologický firmware s atomickou cache ČHMÚ, rychlým rendererem, adaptivním LCD UI a moderním lokálním konfiguračním portálem.

Při rozporu platí tato důkazní hierarchie:

1. opakovaně ověřené chování fyzického zařízení;
2. aktuální zdrojový kód;
3. Git historie a důvody již provedených oprav;
4. externí dokumentace jako podpůrný zdroj a upozornění na rozpor.

Externí stránka sama o sobě neopravňuje ke změně fungujícího hardwarového chování. Typickým příkladem je `EXIO8`: funkční chování se zachová, i když se jeho popis na webu nebo ve schématu liší od pojmenování v projektu.

## 2. Ověřená baseline projektu

- Cílový hardware: ESP32-S3R8, 8 MB OPI PSRAM, 16 MB flash, kulatý RGB LCD 480×480 se ST7701, CST820 a TCA9554.
- Build profil: `esp32:esp32` 3.0.7, Arduino GFX 1.4.9, PNGdec 1.0.1, ArduinoJson 7.1.0, WiFiManager 2.0.17 a ElegantOTA 3.1.6.
- Reprodukční příkaz uvedený projektem: `arduino-cli compile --profile default src`.
- V prostředí při návrhu nebyl `arduino-cli`; nezměněný firmware proto nebyl lokálně přeložen a před implementací musí vzniknout měřená baseline.
- V repozitáři nejsou automatické testy ani skutečná CI konfigurace.
- Hlavní aplikační logika je kooperativní a běží v Arduino `loop()`; vlastní aplikační FreeRTOS úlohy nejsou vytvořené.
- ČHMÚ katalog, šest HTTPS přenosů, PNG dekódování a rendering dnes mohou blokovat UI.
- Současná obnova ČHMÚ přepisuje živé sloty a při částečném selhání může ztratit poslední platnou animaci.
- TLS připojení ČHMÚ používá `setInsecure()`; GeoIP používá HTTP. Bezpečnostní změna se nesmí spojit s migrací Level 1 bez samostatného ověření času a certifikátů.

## 3. Neměnné hardwarové invarianty

Dokud měření nebo reprodukovatelný hardwarový test neprokáže bezpečnější variantu, musí implementace zachovat:

- pořadí inicializace TCA9554 → podsvit vypnutý → ST7701 → první černý flush → podsvit zapnutý;
- současnou ST7701 inicializační sekvenci a RGB pinout;
- pixel clock 8 MHz a současná RGB porch/sync časování;
- dva plné RGB565 framebuffery v PSRAM;
- zero-copy přepnutí viditelného framebufferu při VSYNC;
- dva interní bounce buffery po deseti řádcích;
- současné funkční chování TCA9554 včetně linky označené `EXIO_LCD_PWR`/`EXIO8`;
- vypnutou hardwarovou obnovu CST820 přes expander;
- odmítnutí CST820 vzorků `0xFF`, více než jednoho bodu a souřadnic mimo displej;
- ukončení gesta až po 60 ms souvislého ticha;
- současný dual-app OTA partition layout;
- zhasnutí podsvitu během samotného zápisu OTA do flash;
- debounce zápisu uživatelských hodnot do NVS.

Jedna RGB565 plocha má 480 × 480 × 2 = 460 800 B. Dva panelové framebuffery spotřebují přibližně 900 KiB PSRAM. Při 8MHz PCLK a současném časování panel skenuje přibližně 29,3 snímku/s; jeden scan trvá přibližně 34,2 ms.

## 4. Cílová architektura úrovně 1

Půjde o postupnou modernizaci, nikoli jednorázový přepis nebo migraci na LVGL/čisté ESP-IDF.

### 4.1 Odpovědnosti

- `AppController`: aplikační stav, prioritizace servisních režimů a plánování krátkých operací.
- `ConnectivityManager`: stav Wi-Fi, opakované připojení a backoff.
- `ChmuCatalogParser`: čistý, streamovatelný parser názvů radarových snímků.
- `ChmuTransport`: ohraničený přenos katalogu a PNG, validace HTTP a délky.
- `WeatherFrameStore`: výhradní vlastník `live` a `staging` sady, atomické zveřejnění.
- `WeatherDecoder`: kontrolované PNG dekódování do předem přidělených RGB565 výřezů.
- `Viewport`: čisté převody zeměpisných souřadnic, Web Mercator a mapový výřez.
- `MapAsset` a generátor: reprodukovatelný offline mapový podklad.
- `WeatherRenderer`: přímé řádkové skládání mapy, radaru a UI do zadního framebufferu.
- `DisplayBackend`: jediné místo rozhodující o vlastnictví framebufferu, present/VSYNC a metrikách displeje.
- `Touch_CST820` jako sampler boundary: zachovaná validace fyzických dat CST820 bez nové hardwarové recovery logiky.
- `GestureRecognizer`: stavový automat `tap`, `double tap`, `drag`, `release`.
- `SettingsRepository`: verzovaný model, validace a migrace starého NVS.
- `PortalServer`: časově omezený lokální portál a společná synchronní `WebServer` integrace s OTA.
- `Diagnostics`: pevně velký kruhový buffer událostí a agregované metriky bez alokací v hot path.

Moduly mají používat malé explicitní datové struktury, pevné kapacity a jednoznačné vlastnictví. Velká alokace požadující PSRAM nesmí tiše spadnout do interní RAM.

### 4.2 Aplikační stavy

Minimální stavový model:

- `Booting`
- `Connecting`
- `LoadingWeather`
- `WeatherActive`
- `Settings`
- `Portal`
- `Ota`
- `Degraded`

`Degraded` zachová poslední platná data a zobrazí jejich stáří. OTA má přednost před obnovou počasí; před vstupem do OTA se rozpracovaná staging transakce ukončí a NVS zápis dokončí nebo odloží.

## 5. Weather-only migrace

Odstraní se:

- `ADSB.cpp/.h`;
- `ScreenPlanes.cpp/.h`;
- `EuBorder.cpp/.h`;
- `EuMapData.h`;
- letadlové datové typy, velký ADS-B body buffer a síťový zdroj;
- letecký rozsah, orientace, jednotky, obrazovka, navigace a dokumentace.

Zůstanou společné závislosti a funkce používané počasím, zejména ArduinoJson kvůli GeoIP, HTTP/Wi-Fi, Preferences, Wire, QR kód, displej, dotyk, watchdog a OTA.

Migrace zachová namespace `planeradar`, načte staré klíče a zapíše nové `schema` číslo. Staré `scr=0` (letadla) i neplatné hodnoty se převedou na radar; staré `scr=1` již znamená radar. Wi-Fi údaje spravované ESP32/WiFiManager se nesmějí smazat. Letecké klíče se nejprve jen ignorují.

Aktualizace verzí ESP32 core nebo Arduino GFX není součástí této migrace.

## 6. Atomický datový tok ČHMÚ

Obnova je transakce:

1. stáhnout katalog do ohraničeného stream parseru;
2. syntakticky ověřit názvy `pacz2gmaps3.z_max3d.YYYYMMDD.HHMM.0.png`;
3. seřadit unikátní kandidáty podle UTC;
4. znovu použít snímky již obsažené v platné sadě;
5. nové PNG přijmout pouze do `staging` vlastnictví;
6. ověřit HTTP 200, maximální velikost, skutečně přijaté bajty, deklarovanou délku, PNG signaturu a úspěšné dokončení decoderu;
7. dekódovat požadovanou sadu;
8. jedinou operací zaměnit `staging` za `live`;
9. bývalou `live` sadu bezpečně uvolnit nebo znovu použít.

Renderer čte jen neměnnou `live` sadu. Selhání katalogu, jednotlivého přenosu, PNG, alokace nebo dekódování nesmí změnit zveřejněnou generaci.

Každá sada obsahuje počet rámců, UTC čas a jméno každého rámce, validované komprimované PNG, dekódovaný RGB565 crop, jeho geografické pokrytí, velikost obou bloků, generaci a čas posledního úspěšného zveřejnění. Komprimovaná data se zachovávají proto, aby pan/zoom mohl vytvořit novou atomickou decoded staging sadu bez opětovného stahování. Pokud se nová sada nevejde do PSRAM, stará live sada zůstane beze změny. UI rozlišuje aktuální data, obnovu, zastaralá data a stav bez dat.

Obnova respektuje pětiminutovou kadenci ČHMÚ a používá omezený exponenciální backoff. Timeout musí existovat pro připojení, katalog, hlavičky, jeden PNG i celou transakci. Volání watchdog feed není náhradou timeoutu.

## 7. Renderer, mapa a města

Renderer skládá:

1. mapový podklad;
2. radarovou vrstvu;
3. UI a diagnostické indikátory.

`Viewport` je jediná autorita pro projekci mapy i radarových dat. Při změně viewportu se předpočítají mapovací tabulky `x → sourceX` a `y → sourceY`. Radar se zapisuje přímo po řádcích do RGB565 bufferu bez virtuálního `drawPixel()` a bez dělení v hlavní pixelové smyčce. V animační hot path nejsou alokace.

Během drag gesta se použije poslední decoded coverage a části mimo něj zobrazí pouze mapový podklad. Po uvolnění prstu nebo změně zoomu se z live komprimovaných PNG vytvoří nový decoded staging crop a zveřejní se až jako kompletní sada. Síťové stahování není pro samotný pan/zoom potřeba.

Level 1 mapový asset vznikne offline z auditovatelného zdroje, bude víceúrovňový, celočíselně kvantizovaný a uložený ve firmware. První verze může převést existující `CZ_BORDER`/`CZ_CITIES` beze změny geografického významu; bohatší externí kartografie patří do roadmapy.

Současná implementace měst obsahuje 59 statických položek, dvě priority, plná jména do 50 km, zkratky nad 50 km, jen tier 1 nad 100 km, pozici vždy vpravo a žádnou detekci kolizí. Level 1 musí:

- oddělit data měst od rendereru;
- preferovat méně čitelných názvů před množstvím interních zkratek;
- dovolit přirozené krátké názvy, například `C. Budejovice`, nikoli kód `CB`;
- vybírat několik kandidátních pozic kolem bodu;
- odmítnout kolizi s dříve umístěným textem a s aktivním UI;
- používat kontrastní halo/stín;
- držet stabilní pozici mezi animačními rámci.

Plná česká diakritika a bohatší label engine jsou položkou Next Level, protože vestavěný font je ASCII-only.

## 8. Vlastnictví framebufferu

Stavy panelových bufferů jsou `Free`, `Drawing`, `Ready` a `Visible`. Present se považuje za úspěšný jen po potvrzeném VSYNC. Po timeoutu se lokální index bufferu nesmí slepě přepnout. Událost se zaznamená a současný display watchdog provede ověřený recovery postup.

Dirty rectangles mohou snížit CPU práci, ale ne datový tok RGB panelu; nejsou primární optimalizací. Plný cacheovaný mapový framebuffer stojí dalších přibližně 450 KiB PSRAM a použije se jen po změření rezervy během dvojité `live/staging` cache.

## 9. LCD UI

Výchozí radarový pohled maximalizuje plochu mapy. Trvale zůstává pouze diskrétní čas radarového snímku a závažný stavový indikátor.

Jeden tap zobrazí adaptivní ovládací překryv přibližně na šest sekund:

- stav a čas dat nahoře;
- časová osa dole;
- velká tlačítka přehrát/pozastavit, vystředit a nastavení;
- přiblížení v bezpečné části kruhu.

Dotykové cíle mají nejméně přibližně 48–56 px a neleží v useknutých rozích. Overlay se při aktivitě neschová.

Gesta Level 1:

- tap: ukázat/skrýt overlay;
- drag mapy: pan;
- double tap: zoom kolem místa dotyku;
- drag časové osy: vybrat rámec;
- explicitní tlačítko: návrat domů.

Globální swipe nebude měnit rámce, protože by kolidoval s pan. Long press nebude skrývat kritickou funkci. Jednorázová nápověda gest bude znovu dostupná v nastavení.

LCD nastavení obsahuje pouze volby bez textového vstupu: jas, výchozí rozsah, rychlost a pauzu animace, počet rámců, viditelnost stavových prvků, timeout overlaye, návrat domů, spuštění portálu, OTA a diagnostiku.

## 10. Moderní lokální portál

Portál je responzivní, použitelný na mobilu v captive režimu a nemá runtime závislost na internetu. Nepoužívá CDN, externí fonty ani externí JavaScript. Nejde o velké SPA; server renderuje malý shell a JSON endpointy poskytují dynamický stav.

Sekce:

1. Přehled — Wi-Fi, RSSI, IP, čas/stáří dat, firmware, paměť, poslední chyba.
2. Připojení a poloha — scan sítí, připojení, GeoIP, ruční souřadnice, domov a zobrazení aktuálního časového pásma.
3. Radar a displej — počet rámců, rychlost/pauza, stale policy, ruční jas, overlay a mapové vrstvy. Noční/automatický jas zůstává v roadmapě.
4. Systém — OTA, restart, diagnostika, reset Wi-Fi a tovární reset.

Heslo se nevrací v HTML/JSON/logu. Veškeré vstupy se validují serverem. Destruktivní akce používají dvoustupňové potvrzení. Portál běží jen při explicitním servisním režimu nebo prvním provisioning flow a má timeout.

ElegantOTA používá stejný synchronní `WebServer`; nepotřebné Async WebServer/Async TCP knihovny se odstraní až po sestavení dokazujícím, že nejsou potřeba. Během flash write zůstává displej zhasnutý kvůli známé PSRAM/cache interakci.

## 11. Souběh a odezva

Nejprve vzniknou čisté hranice, testy a měření. Pokud TLS/HTTP blokuje reakci UI déle než 100 ms, Level 1 může přidat právě jednu omezenou síťovou FreeRTOS úlohu:

- vlastní HTTP klienta a komprimované staging buffery;
- nikdy nevlastní displej, dotyk nebo viditelný framebuffer;
- předává malé zprávy a dokončené vlastnictví bufferu;
- používá pevnou frontu bez hot-path alokace;
- ukončí se potvrzeným handshake před OTA.

Dekódování a rendering zůstanou nejprve v hlavní úloze. Jejich přesun vyžaduje nové měření PSRAM contention. FreeRTOS není povinný cíl sám o sobě; je podmíněná odpověď na měřenou latenci.

## 12. Nastavení a jas

Level 1 zachová ruční PWM jas a přidá validované hodnoty pro UI, animaci a mapu. Zápisy jsou debounced a plánují se mimo kritický frame present.

Automatický jas podle Slunce je Next Level. Vyžaduje NTP nebo ověřenou RTC, výpočet východu/západu/soumraku, plynulý přechod, minimální bezpečný jas a ruční override. Neplatný čas vždy vede k uloženému ručnímu jasu, nikdy k nečekanému zhasnutí.

## 13. Testovací strategie

### Host C++

- parser katalogu a filename;
- UTC řazení a deduplikace;
- settings validation/migration;
- `WeatherFrameStore` commit/abort a failure injection;
- projekce/viewport;
- gesture state machine;
- city label placement/collision;
- framebuffer ownership state machine.

### Python/golden

- reprodukovatelnost mapového generátoru;
- bounds a počty prvků;
- RGB565 renderer → PNG;
- referenční obrazovky pro rozsahy, overlay, stale/offline a error stavy.

### Hardware

- studený start a restarty;
- LCD/VSYNC a dotykový šum;
- Wi-Fi drop/reconnect;
- neúplný a poškozený PNG;
- NVS zápis během běhu RGB;
- OTA success/failure/exit;
- 24h a následně 72h soak.

Metriky: velikost firmware, volná/minimální/největší interní RAM a PSRAM, katalog/download/decode/compose/present časy, zmeškané VSYNC, stáří live dat a restart reason.

## 14. Kritéria dokončení Level 1

- Letecký kód, UI a data nejsou v binárním obrazu.
- Migrace zachová Wi-Fi, polohu, jas a meteorologický rozsah.
- Selhání obnovy nezmění poslední platnou generaci.
- UI reaguje během síťových operací do 100 ms nebo je zdokumentována měřená výjimka schválená před releasem.
- Framebuffer se nepřepíná po nepotvrzeném VSYNC.
- Města jsou čitelná, nekolidují s aktivním overlayem a používají schválené 1:1 náhledy.
- Portál funguje z telefonu bez internetu a neprozrazuje heslo.
- Host, golden, Arduino build a definované hardwarové scénáře procházejí.
- 24h a 72h soak nemají klesající trend největšího volného bloku, nečekaný reboot ani ztrátu displeje.
- Skutečné výsledky jsou zapsané vůči baseline.

## 15. Mimo rozsah Level 1

Položky jako automatický jas, plná diakritika, detailní okolní mapa, další vrstvy, crossfade, export/import nastavení a rozšířený FreeRTOS pipeline jsou zachované v [Next Level roadmapě](./2026-08-08-meteolcd-next-level-roadmap.md), nikoli zapomenuté nebo neformálně přimíchané do prvního releasu.
