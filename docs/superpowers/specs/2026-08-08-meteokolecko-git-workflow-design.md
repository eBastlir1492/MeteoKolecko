# MeteoKolecko Git a GitHub workflow — návrhová specifikace

**Datum:** 2026-08-08
**Stav:** Schválený návrh čekající na provedení

## 1. Cíl

Napojit současný lokální projekt na GitHub fork `eBastlir1492/MeteoKolecko`, zachovat kompletní historii původního `petus/MeteoPlaneRadar`, zabránit nechtěnému pushi do upstreamu a vytvořit větvení, ve kterém lze po přerušení nebo ztrátě kontextu jednoznačně pokračovat.

Historie se nebude squashovat, přepisovat ani nahrazovat orphan větví. Původní commity, tagy, licence a autorství zůstávají dohledatelné.

## 2. Ověřený výchozí stav

Lokální repozitář:

- větev `main`;
- HEAD `5b0ab25` — `docs: add MeteoLCD architecture and implementation plans`;
- čistý worktree;
- původní remote je nyní pojmenovaný `origin` a ukazuje na `https://github.com/petus/MeteoPlaneRadar.git`;
- lokální `main` sleduje původní `origin/main`;
- původní remote tip je `fcf0ced215632be0bc3ca42b0b0b8468a850b430`;
- lokální historie má 36 commitů a lokální tagy `v0.4`, `v0.5.2`, `v0.5.3`, `v0.5.4`.

Nový GitHub fork:

- URL `https://github.com/eBastlir1492/MeteoKolecko.git`;
- výchozí větev `main`;
- `main`/HEAD je `fcf0ced215632be0bc3ca42b0b0b8468a850b430`;
- fork byl vytvořen pouze s výchozí větví a při čtecím ověření nepublikoval tag `v0.5.4`.

Před vytvořením této workflow specifikace byl lokální `main` očekávaný fast-forward nástupce forku o jediný dokumentační commit. Commit této specifikace přidá druhý lokální dokumentační commit. Pro push není potřeba `--force`.

## 3. Cílové remoty

Po přepojení musí platit:

```text
origin    https://github.com/eBastlir1492/MeteoKolecko.git
upstream  https://github.com/petus/MeteoPlaneRadar.git
```

`origin` je jediný běžný push cíl. `upstream` slouží pouze pro fetch a porovnávání.

Současný `origin` se přejmenuje na `upstream`, aby se neztratil žádný fetch ref nebo související konfigurační údaj. Poté se přidá nový `origin`.

Pro `upstream` se nastaví záměrně nefunkční push URL `DISABLED`. Tím běžný `git push upstream ...` selže ještě před pokusem o síťovou autentizaci. Fetch URL zůstane platná.

Lokální konfigurace po prvním pushi:

```text
branch.main.remote = origin
branch.main.merge = refs/heads/main
remote.pushDefault = origin
```

## 4. Bezpečný postup přepojení

1. Ověřit čistý worktree a větev `main`; `5b0ab25` i commit této workflow specifikace musí být v aktuální historii.
2. Ověřit, že `fcf0ced` je předkem lokálního HEAD.
3. Přejmenovat současný `origin` na `upstream`.
4. Přidat nový `origin` s URL forku.
5. Zakázat push URL upstreamu.
6. Provést `git fetch --prune` zvlášť pro `origin` a `upstream`.
7. Znovu ověřit, že `origin/main` je přesně `fcf0ced` a je předkem lokálního `main`.
8. Zkontrolovat `git log --graph` a rozdíl `origin/main..main`; očekávají se pouze schválené dokumentační commity vytvořené tímto procesem, počínaje `5b0ab25`. Jakákoli změna mimo `docs/` postup zastaví.
9. Pushnout `main` běžným `git push -u origin main`.
10. Ověřit shodu lokálního `main` a `origin/main` podle object ID.
11. Porovnat lokální a vzdálené tagy a pushnout původní tagy pouze bez přepisování.
12. Nastavit `remote.pushDefault=origin` a znovu vypsat efektivní konfiguraci.
13. Vytvořit `feature/level1-phase-a` z ověřeného `main` a publikovat ji s upstream trackingem na `origin`.

Zakázané operace:

- `git push --force` a `--force-with-lease`;
- rebase nebo reset publikovaného `main`;
- orphan větev nebo odstranění původních commitů;
- mazání či přepis původních tagů;
- automatické sloučení upstreamu bez samostatné revize.

Pokud jakákoli kontrola před pushnutím ukáže neočekávaný commit nebo rozvětvení, postup se zastaví. Remoty lze bezpečně opravit bez změny pracovních souborů; historie se nebude násilně sjednocovat.

## 5. Branch strategie Level 1

`main` je stabilní, sestavitelná a zveřejnitelná větev MeteoKolecko. Schválená dokumentace na commitu `5b0ab25` je její první vlastní změna.

Implementace se rozdělí podle existujících fázových plánů:

```text
feature/level1-phase-a
feature/level1-phase-b
feature/level1-phase-c
feature/level1-phase-d
feature/level1-phase-e
```

Pravidla:

- v jednu chvíli je aktivní pouze jedna fázová větev;
- každá vznikne z aktuálního `main` až po dokončení předchozí fáze;
- jeden task ve fázovém plánu odpovídá jednomu ověřenému commitu;
- checkboxy dokončeného tasku se commitují společně s jeho změnou;
- rozpracovaný task se neposouvá na další větev;
- po ověření celé fáze se větev sloučí do `main` explicitním merge commitem;
- další fáze začne až z takto aktualizovaného `main`;
- vzdálená fázová větev se může po merge odstranit, lokální Git historie ji nadále obsahuje.

Tři úrovně obnovy práce:

1. první nezaškrtnutý krok ukazuje další krátkou akci;
2. poslední task commit ukazuje poslední úplný implementační checkpoint;
3. poslední phase merge na `main` ukazuje poslední úplnou a ověřenou fázi.

Při obnovení se vždy kontroluje `git status --short`, aktuální větev, posledních osm commitů a příslušný fázový plán. Nečistý worktree se nikdy automaticky nemaže nebo resetuje.

## 6. Pull requesty a ochrana main

Po prvním pushi se na GitHubu pro `main` zakáže force-push a smazání větve.

Fázová větev může být sloučena přes GitHub pull request, což je preferované kvůli přehlednému review a historii. Povinné CI checks se nenastaví, dokud v repozitáři neexistuje skutečná workflow, která umí pinned Arduino build a relevantní testy. Neexistující nebo nefunkční kontrola nesmí blokovat repozitář jen kvůli zdání ochrany.

Jakmile bude CI vytvořená a ověřená, pravidla `main` se rozšíří o:

- merge pouze přes pull request;
- úspěšné host testy;
- úspěšný Arduino compile s profilem `default`;
- zákaz force-push a smazání.

## 7. Tagy a verze

Původní tagy `v0.4`, `v0.5.2`, `v0.5.3` a `v0.5.4` zůstávají ukazateli původních MeteoPlaneRadar vydání. Nesmějí se přesouvat na nové commity.

MeteoKolecko použije odlišný namespace:

```text
meteolcd-v0.1.0
meteolcd-v0.2.0
...
```

První nový release tag vznikne až po splnění Level 1 release kritérií. Samotné plánování nebo neověřený mezistav se netaguje jako release.

## 8. Upstream synchronizace

Během Level 1 se upstream nebude automaticky slučovat. Nový upstream commit se nejprve vyhodnotí podle rozdílu a historie důvodu změny.

Potřebná upstream oprava dostane větev:

```text
chore/sync-upstream-YYYYMMDD
```

Na ní se změna cherry-pickne nebo ručně přenese, sestaví a prověří proti hardwarovým invariantům. Zvláštní pozornost mají `Display_ST7701`, `Canvas16`, `Touch_CST820`, `TCA9554`, OTA a partition layout. Teprve ověřená změna se sloučí do `main` nebo právě aktivní fáze.

## 9. Autentizace a citlivé údaje

Na počítači je aktivní Git Credential Manager (`credential.helper=manager`). První HTTPS push může otevřít bezpečný GitHub login. Token, heslo ani recovery kód se nikdy neposílá do chatu, neukládá do repozitáře a nezobrazuje v logu.

GitHub CLI není nainstalované a pro samotné přepojení není nutné. Instalace dalšího nástroje nebude podmínkou, pokud Git Credential Manager úspěšně autentizuje běžný Git push.

## 10. Přijetí změny

Git přepojení je hotové pouze tehdy, když:

- worktree je čistý;
- `origin` a `upstream` mají správné fetch URL;
- upstream push URL je `DISABLED`;
- `main` sleduje `origin/main`;
- lokální `main` a `origin/main` ukazují na stejný aktuální workflow commit, který je potomkem `5b0ab25`;
- původní Git historie je dosažitelná z `main`;
- na forku existují původní tagy bez změny object ID;
- `feature/level1-phase-a` existuje lokálně i na `origin`;
- nebyl použit force-push;
- `git log --graph --decorate --all` neukazuje neočekávanou paralelní root historii.
