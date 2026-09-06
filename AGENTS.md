# RTLPlayground: zasady pracy z fizycznym switchem

## Najważniejsze ograniczenia użytkownika

- Nie wolno zbrickować urządzenia. Użytkownik nie chce lutować ani odzyskiwać urządzenia przez programator. Nie obiecuj zerowego ryzyka: jeżeli operacja może wymagać takiego odzyskiwania, zatrzymaj wdrożenie i wyjaśnij konkretną przeszkodę.
- Nigdy nie zeruj ani nie nadpisuj ustawień użytkownika: VLAN, management VLAN, PVID, tagging, IP, maski, bramy, MAC, hasła, portów i pozostałej konfiguracji. Nie używaj resetu fabrycznego, konfiguracji domyślnej ani importu repozytoryjnego `config.txt` na urządzeniu.
- Prace lokalne i diagnostyka są dozwolone. Ogólna zapowiedź przyszłego modyfikowania i wgrywania nie oznacza zgody na wgranie dowolnego obrazu teraz.
- Nie restartuj ani nie wyłączaj switcha w ramach diagnostyki. Niezapisana konfiguracja może zniknąć po restarcie.
- Nigdy nie wykonuj `git push`, force-push, publikacji gałęzi ani tworzenia/aktualizacji PR bez wyraźnego żądania dokładnie tej operacji w bieżącej rozmowie. Po lokalnej weryfikacji zgłoś gotowość do push; nie publikuj samodzielnie.

## Urządzenie i dostęp

- Adres: `192.168.5.9`, HTTP na porcie 80. Hasło użytkownik podał w rozmowie; nie zapisuj go w repozytorium, logach ani argumentach procesów. Narzędzie diagnostyczne pyta o hasło lub czyta `SWITCH_PASSWORD` ze środowiska.
- Odczyt 2026-09-05: `SWTGW218AS 8+1 Managed Switch`, target `MACHINE=SWTGW218AS`, firmware `v0.1.0-8a7b146`, flash zgłoszony jako `2 MB` (2 MiB).
- Adresacja zastana: `192.168.5.9/24`, brama `192.168.5.1`. Management VLAN: `1`; lista aktywnych VLAN: `1`, `4` (`smart_local`), `5` (`smart_wan`). To stan zastany, nie instrukcja konfiguracji.
- Pierwszy eksport `/config` zawierał tylko 52 bajty: IP, maskę i bramę, bez aktywnych VLAN. Po zapisaniu ustawień przez użytkownika snapshot `20260905T211603.522311Z-192.168.5.9` zawiera również VLAN 4 (`smart_local`) i 5 (`smart_wan`), oba tagowane na portach 1–9. Zapisane ustawienia sieci odpowiadają odczytanemu stanowi aktywnemu.
- Brak jawnej komendy VLAN 1 w tym eksporcie jest zgodny z kodem startowym: `vlan_setup()` tworzy VLAN 1 z portami nietagowanymi i PVID 1, a `management_vlan` zaczyna od 1. Te wartości odpowiadają snapshotowi. Ostrzeżenie narzędzia o brakującym VLAN 1 nie uwzględnia tych wartości domyślnych. To potwierdzenie adresacji i VLAN-ów, nie dowód pełnej odtwarzalności wszystkich ustawień ani dump flash.
- `httpd/page_impl.c:send_config()` używa `flash_region.len` po `flash_read_bulk()`, które zmienia tę długość. Eksport wymaga ostrożnej weryfikacji; HTTP 200 nie dowodzi kompletności kopii.
- 2026-09-05, po ponowionym jednoznacznym poleceniu użytkownika i wyjaśnieniu braku odzyskiwania bez programatora, wgrano poprawkę logowania z `output/SWTGW218AS-login-fix/`. SHA-256 obrazu: `e9e36f3a93153334173b31cf8fafa23e2efe0cc5ec625009532e851b4b9f836f`. Switch potwierdził CRC i wrócił pod `192.168.5.9`. Sprawdzono na urządzeniu zgodność strony logowania z buildem, logowanie bez obejścia, widok portów oraz identyczność zapisanej konfiguracji i aktywnych VLAN-ów 1, 4, 5 wraz z PVID. Zgoda dotyczyła tej konkretnej poprawki; nie jest zgodą na przyszłe aktualizacje.

## Diagnostyka

- Najnowszy wgrany obraz (2026-09-06): `output/SWTGW218AS-ui-autosave-20260906/rtlplayground-v0.1.0-8a7b146-dirty-SWTGW218AS.bin`, SHA-256 `24bb66692b7ea1d5bfbd7fa636f8354a6f9c709ecf509117724ae57d99e1375e`. Na polecenie aktualizacji zachowano też nowe aktywne wyłączenie portów 3–5, dopisując tylko te trzy komendy do niezmienionego wcześniejszego eksportu i sprawdzając odczyt przed restartem. Snapshot po aktualizacji `20260906T001049.093353Z-192.168.5.9` potwierdza zachowanie konfiguracji i porównanych statycznych ustawień; porty 3–5 nadal są wyłączone. Oba logowania, nowy nagłówek, opcja autosave i ręczny eksport sprawdzone na urządzeniu. Autosave pozostaje domyślnie wyłączoną preferencją przeglądarki.

- Najnowsza aktualizacja z 2026-09-06, na osobne polecenie „wgraj caveman mode”: `output/SWTGW218AS-ui-interactive-20260906/rtlplayground-v0.1.0-8a7b146-dirty-SWTGW218AS.bin`, SHA-256 `12bfc39af9c87ed9a7540816c2548178e0c5b7d8ad79e82e0fece3a6bb044805`. Obejmuje interaktywne oznaczenia portów i potwierdzenia przed utratą szkiców. Switch potwierdził CRC. Snapshot po aktualizacji `20260905T232920.748953Z-192.168.5.9` potwierdza zachowanie konfiguracji zapisanej, VLAN-ów/PVID i porównanych statycznych ustawień. Zwykła przeglądarka przeszła oba logowania oraz widoki portów/VLAN; oddzielne odczyty HTTP potwierdziły bajtową zgodność głównych zasobów. Sporadyczne timeouty nadal występują.

- Aktualizacja UI z 2026-09-06, wykonana na osobne polecenie „wgraj”: obraz `output/SWTGW218AS-ui-serial-20260906/rtlplayground-v0.1.0-8a7b146-dirty-SWTGW218AS.bin`, SHA-256 `7eea79f92219796397ce453713af07a247d51644cfe5a818577c581d0e3157d3`. Switch potwierdził CRC. Snapshot po aktualizacji `20260905T231251.911562Z-192.168.5.9` potwierdza zachowanie konfiguracji zapisanej, VLAN-ów/PVID i porównanych statycznych ustawień portów, EEE, STP, agregacji, mirroringu, MTU i bandwidth. Zwykła przeglądarka potwierdziła oba logowania, widok portów i inline edytor VLAN oraz zgodność zasobów z buildem. Sporadyczne timeouty HTTP nadal występują. Późniejsze lokalne zmiany UI nie są automatycznie częścią tego wgranego obrazu.

- Używaj `make -f Makefile.switch help`. Domyślny cel tylko wyświetla pomoc; żaden cel nie wgrywa firmware ani konfiguracji.
- `status` / `storage`: logowanie przez POST `/login`, potem GET `/information.json`. `snapshot`: wyłącznie jawnie dozwolone odczyty stanu i konfiguracji. Żądania pojedynczo, z timeoutem, bez automatycznych przekierowań i ponawiania.
- Sam GET nie gwarantuje bezpieczeństwa: `/reset`, `/l2_del.json` i `/cmd_log_clear` zmieniają stan. Nie wywołuj ich. Nie używaj POST `/cmd`, `/config`, `/upload` do diagnostyki ani arbitralnych poleceń konsoli.
- Kopie w `.switch-backups/` są lokalne, ignorowane przez Git, z ograniczonymi prawami i sumami SHA-256. Mogą zawierać hasło. Nie dodawaj ich do commitów i nie traktuj snapshotu jako pełnego dumpu flash ani automatycznie odtwarzalnej konfiguracji.
- RAM sprawdzaj lokalnie z raportów SDCC `.mem` / `.map`. HTTP nie udostępnia pomiaru wolnego RAM w czasie pracy. Układ ma 256 B IRAM oraz 64 KiB XRAM; rezerwa stosu w raporcie linkera nie jest pomiarem maksymalnego użycia stosu.

## Budowanie i przyszłe wdrożenia

- `html/` jest ignorowanym wynikiem budowania, nie źródłem. `make MACHINE=SWTGW218AS` sam instaluje zależności z lockfile, buduje frontend w `web/`, generuje HTML i pakuje go do firmware. `make frontend` buduje tylko UI. Wymagane Node.js 22.12+ i npm; zaktualizowany Dockerfile je zawiera. Lokalny obraz środowiska budowania: `rtlplayground-ui-build:latest`. Pierwsza instalacja npm wymaga sieci; późniejsze budowanie może korzystać z istniejących zależności bez sieci. Polecenia build nadal nie kontaktują się ze switchem.

- Przeglądaj `README.md`, `doc/hardware.md`, `Makefile`, `machine.c`, `httpd/httpd.c`, `rtlplayground.c` i `rtl837x_common.h` przed zmianami zależnymi od sprzętu.
- Używaj SDCC 4.5 i jawnego `MACHINE=SWTGW218AS`. Budowanie i symulator lokalny nie mogą mieć efektów ubocznych na switchu. Nie zmieniaj targetu na podstawie podobnej liczby portów.
- Sprawdzaj IRAM, stos, XRAM, banki kodu, obszar HTML, długość obrazu i CRC. Zachowuj zapas; pomyślny build i CRC nie dowodzą bezpieczeństwa działania na sprzęcie.
- Układ flash w tym commicie: domyślna konfiguracja `0x6f000`, konfiguracja użytkownika `0x70000..0x70fff`, staging `0x80000..0xfffff`, obraz 524288 B. Updater kopiuje zakres poniżej `0x70000`, pozostawiając sektor użytkownika. Nie zmieniaj tych granic ani logiki zachowania konfiguracji bez pełnej analizy.
- Aktualizacja najpierw trafia do staging, następnie nadpisuje firmware aktywny sektor po sektorze. Nie jest to A/B z automatycznym rollbackiem; utrata zasilania podczas kopiowania może zbrickować urządzenie. Kopia ustawień nie usuwa tego ryzyka.
- Nie używaj obrazu OEM z `installer/` na urządzeniu działającym już pod RTLPlayground. Nie wykonuj bezpośredniego kasowania/zapisu flash, zmian bootowania, zegarów, PHY, GPIO ani rejestrów jako eksperymentu na urządzeniu użytkownika.
- Przed rozważeniem wdrożenia wymagane są: pełna kopia konfiguracji aktywnej i zapisanej, wyjaśnienie ich różnic, zgodność sprzętu, przegląd zmian i raportów pamięci, lokalna walidacja obrazu oraz wykazana ścieżka odzyskania zgodna z wymaganiem użytkownika. Dopóki nie ma takiej ścieżki, przygotowuj i testuj lokalnie, ale nie flashuj.
