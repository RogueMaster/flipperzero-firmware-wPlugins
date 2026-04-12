# PCB v1.1 Design Notes

## Cel zmian

Wersja 1.1 ma rozwiazac trzy glowne problemy obecnej plytki:

1. Ograniczenie slyszalnych zaklocen z linii 5.25 V Flippera w torze audio.
2. Zastapienie PAM8403 ukladem PAM8406 https://www.diodes.com/assets/Datasheets/PAM8406.pdf https://www.diodes.com/assets/Evaluation-Boards/PAM8406-User-Guide.pdf, ktory jest aktywnym produktem i daje dodatkowo wybor trybu Class D / Class AB.
3. Dodanie sprzetowych linii sterujacych wzmacniaczem, aby aplikacja FM mogla sterowac nie tylko PT, ale tez sama koncowka mocy.
4. Dodanie osobnych LDO 3.3 V dla sekcji PT i TEA, aby odseparowac je od zaklocen generowanych przez koncowke mocy i zasilanie 5.25 V Flippera.

## Architektura zasilania v1.1

Zrodlo:
- 5.25 V z Flippera

Glowne filtrowanie wejscia audio:
- L1: 120 uH, ekranowany
- C1: 470 uF elektrolit do GND za L1

Z punktu za L1 i C1 wychodza trzy osobne galezie:
1. galaz zasilania PAM8406
2. galaz LDO TPS7A2033 3.3 V dla PT
3. galaz LDO TPS7A2033 3.3 V dla TEA

To ma byc topologia gwiazdy. Nie wolno prowadzic TEA, PT i PAM szeregowo z jednej galezi.

## Ograniczenia Flipper Zero

Aktualne ograniczenia z dokumentacji Flippera:
- pin 1, 5 V: maksymalny prad obciazenia 1.2 A
- pin 9, 3.3 V: maksymalny prad obciazenia 1.2 A
- pojedynczy pin GPIO: do 20 mA
- logika GPIO Flippera pracuje na 3.3 V

Uwaga:
- gdy Flipper jest pod USB, 5 V na pinie 1 pochodzi bezposrednio z USB
- gdy Flipper pracuje z baterii, 5 V na pinie 1 trzeba recznie wlaczyc w GPIO app
- duze obciazenie na 5 V bedzie najbardziej krytyczne dla maksymalnej glosnosci koncowki mocy

## Szacowanie mocy dla stereo 8 om

Docelowe glosniki:
- 2 x 8 om stereo

Przy zasilaniu 5.0 V idealny limit mocy BTL dla jednego kanalu wynosi w przyblizeniu:

- Pmax ~= VDD^2 / (2 x R)
- Pmax ~= 5.0^2 / (2 x 8) = 25 / 16 = 1.56 W na kanal

Przy zasilaniu 5.25 V idealny limit wynosi:

- Pmax ~= 5.25^2 / 16 = 27.56 / 16 = 1.72 W na kanal

To sa wartosci idealne, blisko granicy przesterowania. Realnie nalezy zakladac mniej:
- okolo 1.2 W do 1.5 W na kanal przy 8 om
- razem okolo 2.4 W do 3.0 W audio w stereo

Przy sprawnosci klasy D rzedu 85 do 90 procent oznacza to orientacyjnie:
- pobor mocy z 5 V okolo 2.7 W do 3.5 W w typowym glosnym graniu
- maksymalnie okolo 4 W przy bardzo wysokim poziomie
- sredni prad z linii 5 V okolo 0.55 A do 0.8 A

Wniosek:
- stereo 8 om bedzie dzialac
- nie bedzie to 3 W na kanal, bo to dotyczy obciazenia 4 om
- z punktu widzenia budzetu 5 V Flippera jest to wykonalne, ale trzeba uwazac na szczytowe obciazenie, rozruch i spadki napiecia przy bardzo glosnym odsluchu
- filtr 120 uH + 470 uF oraz poprawne prowadzenie masy pozostaja krytyczne

## Dobor elementow zasilania

### Glowny filtr 5 V

L1:
- 120 uH
- ekranowany
- Isat minimum 2.5 A, zalecane 3 A
- niski DCR, najlepiej ponizej 0.15 om

C1:
- 470 uF
- 10 V lub 16 V
- zwykly low-ESR elektrolit
- nie trzeba ekstremalnie niskiego ESR

Uzasadnienie:
- przy 120 uH i 470 uF rezonans wypada okolo 670 Hz
- to odpowiada dolnej granicy obserwowanego pasma zaklocen zaczynajacego sie okolo 650 Hz
- filtr zaczyna dzialac przed glownym pikiem 1031 do 1043 Hz

## LDO dla TEA i PT

Zalecenie:
- osobny LDO TPS7A2033 dla TEA
- osobny LDO TPS7A2033 dla PT

Preferowany model:
- TPS7A2033

Powod:
- niski szum
- dobry PSRR
- sensowny wybor do analogowego toru audio i tunera FM

Dopuszczalny tanszy zamiennik:
- AP7333-33

Ale dla TEA lepiej trzymac TPS7A2033.

### Kondensatory przy kazdym LDO

Na wejsciu kazdego LDO:
- 10 uF X7R
- 100 nF X7R

Na wyjsciu kazdego LDO:
- 10 uF X7R
- 100 nF X7R

Uwaga:
- samo 1 uF na wejsciu LDO nie jest zalecane jako jedyny lokalny bufor
- po duzym dlawiku zrodlo zasilania robi sie bardziej miekkie, wiec LDO powinien miec lokalne kondensatory wejsciowe

## Wnioski z datasheet TPS7A2033 / TPS7A20

Ponizsze wnioski sa wyciagniete z fragmentu dokumentacji TI dostarczonego do projektu i odnosza sie do uzycia LDO w naszej architekturze zasilania.

Lokalna referencja obrazkowa layoutu:
- [pdfimages/PAM8406-User-Guide/tps7a20_pcb_layout.png](pdfimages/PAM8406-User-Guide/tps7a20_pcb_layout.png)
- [pdfimages/PAM8406-User-Guide/tps7a20_schematic.png](pdfimages/PAM8406-User-Guide/tps7a20_schematic.png)

Podglad inline w GitHub Markdown:

![TPS7A20 PCB layout](pdfimages/PAM8406-User-Guide/tps7a20_pcb_layout.png)

![TPS7A20 schematic](pdfimages/PAM8406-User-Guide/tps7a20_schematic.png)

Referencja PDF producenta:
- https://www.ti.com/lit/ds/symlink/tps7a20.pdf

### Typy kondensatorow

TI zaleca:
- ceramiczne kondensatory o niskim ESR,
- dielektryk X7R, X5R albo C0G,
- nie stosowac Y5V.

Wazna uwaga praktyczna:
- efektywna pojemnosc ceramika pod napieciem i temperatura moze spasc nawet o okolo 50%,
- dlatego wartosci nominalne trzeba dobierac z zapasem,
- nasze zalozenie 10 uF + 100 nF na wejsciu i wyjsciu LDO jest bezpieczne i bardziej konserwatywne niz minimum katalogowe.

### Minimalne wymagania IN i OUT

Z dokumentacji:
- LDO moze byc stabilne nawet bez kondensatora wejsciowego,
- ale dobra praktyka analogowa wymaga kondensatora od IN do GND,
- TI zaleca co najmniej wartosc z tabeli warunkow pracy, w typowej aplikacji minimum 1 uF na wejsciu i 1 uF na wyjsciu.

Wniosek dla naszego projektu:
- nie schodzic ponizej 1 uF na IN i 1 uF na OUT,
- pozostawic nasze zalozenie 10 uF X7R + 100 nF X7R na IN,
- pozostawic 10 uF X7R + 100 nF X7R na OUT.

Powod:
- za glownym dlawikiem 120 uH zrodlo dla LDO nie jest twarde,
- TI wprost pisze, ze przy wiekszej impedancji zrodla niz 0.5 Ohm lub przy dluzszej odleglosci od zrodla zasilania trzeba dac kondensator IN,
- w przeciwnym razie mozliwe sa ringing, oscylacje i degradacja PSRR.

### Reakcja na szybkie zmiany obciazenia

Z dokumentacji TI:
- wieksza pojemnosc na wyjsciu zmniejsza piki przy skokach obciazenia,
- ale spowalnia odpowiedz ukladu,
- duzy skok obciazenia rozladowuje najpierw kondensator wyjsciowy, a dopiero potem LDO nadgania pradem.

Wniosek dla nas:
- 10 uF na wyjsciu LDO jest dobrym kompromisem,
- nie ma potrzeby pakowac bardzo duzych pojemnosci bezposrednio na wyjsciu TPS7A2033,
- dodatkowe lokalne odsprzeganie przy TEA i PT pozostaje wskazane.

### UVLO i zachowanie przy zapadach zasilania

Z dokumentacji:
- LDO ma UVLO,
- przy zapadzie napiecia ponizej progu UVLO uklad sie wylacza,
- po powrocie napiecia ponad prog rising threshold nastepuje normalny start.

Wniosek dla nas:
- przy duzych pikach obciazenia z 5 V Flippera moze dojsc do chwilowego pogorszenia regulacji,
- dlatego odseparowanie TEA i PT przez osobne LDO i dobry bufor na IN jest sensowne,
- to kolejny argument za duzym filtrem 120 uH + 470 uF przed rozgalezieniem zasilania.

### Straty mocy i termika

TI podaje przyblizenie:
- PD = (VIN - VOUT) x IOUT

Dla naszego przypadku orientacyjnie:
- VIN ~= 5.25 V,
- VOUT = 3.3 V,
- spadek na LDO ~= 1.95 V.

Przykladowe straty:
- dla 20 mA: okolo 39 mW,
- dla 50 mA: okolo 98 mW,
- dla 100 mA: okolo 195 mW.

Wniosek:
- dla TEA5767 i PT2257/PT2259 straty sa male i SOT-23 jest wystarczajace,
- ale nie nalezy probowac z tego LDO zasilac koncowki mocy,
- wokol LDO trzeba zostawic miedz i nie upychac obok zrodel ciepla.

### Uwagi PCB dla wersji SOT-23

Najwazniejsze zalecenia z dokumentacji TI, przelozone na nasz projekt:
1. Kondensator IN i OUT umiescic maksymalnie blisko LDO.
2. Polaczenia miedzy LDO i kondensatorami zrobic bardzo krotkie.
3. Dac lokalna miedz na GND, IN i OUT dla poprawy termiki i malej impedancji.
4. Nie prowadzic dlugich, cienkich sciezek zasilania do LDO.
5. Nie stawiac LDO blisko sekcji PAM8406 i pradow wyjsciowych glosnika.

Uwaga dla SOT-23:
- fragment TI wspomina o thermal pad i viach dla innych obudow z exposed pad,
- dla naszej wersji SOT-23 najwazniejsze jest po prostu dobre pole miedzi i bliski placement kondensatorow,
- nie ma potrzeby interpretowac uwag o exposed pad jako wymagania dla SOT-23.

### Finalna rekomendacja dla naszych LDO

Kazdy z dwoch LDO TPS7A2033 powinien miec:
- CIN = 10 uF X7R + 100 nF X7R,
- COUT = 10 uF X7R + 100 nF X7R,
- placement kondensatorow bezposrednio przy pinach,
- osobna czysta mase analogowa,
- krotka trase od punktu po filtrze 120 uH + 470 uF.

To jest bardziej konserwatywne niz minimum z datasheetu TI i dobrze pasuje do naszego problemu z zakloceniami oraz do zasilania z Flippera.

## Sekcja PAM8406

PAM8406 ma byc zasilany bezposrednio z odfiltrowanej galezi 5 V za L1 i C1, bez dodatkowego LDO.

Przy samym ukladzie PAM8406:
- C2: 10 uF X7R miedzy VDD i GND
- C3: 100 nF X7R miedzy VDD i GND
- C4: 1 uF X7R miedzy VREF i AGND

Opcjonalnie:
- dodatkowe 22 uF X7R przy lokalnym VDD PAM8406, jesli miejsce na PCB pozwala

Uwaga projektowa:
- 10 uF i 100 nF maja byc maksymalnie blisko pinow zasilania ukladu
- 1 uF VREF ma byc maksymalnie blisko pinu VREF
- sciezka VREF i jego masa nie moga isc razem z powrotem pradu glosnikowego

## Zalecane polaczenie zasilania

Schemat logiczny:

5.25 V Flipper
-> L1 120 uH
-> wspolny czysty punkt zasilania audio
-> C1 470 uF do GND
-> galaz 1: PAM8406
-> galaz 2: LDO 3.3 V dla PT
-> galaz 3: LDO 3.3 V dla TEA

Kazda z trzech galezi ma wychodzic osobno z punktu za C1.

## Zasady prowadzenia masy

To jest krytyczne.

1. Masa mocy PAM8406 ma wracac osobna sciezka do punktu gwiazdy.
2. Masa wejscia audio PAM8406 ma wracac osobna sciezka jako czysta masa sygnalowa.
3. Masa TEA i PT ma isc po stronie analogowej, nie przez prady koncowki mocy.
4. Polaczenie masy mocy i masy sygnalowej ma byc w jednym kontrolowanym punkcie, najlepiej przy glownym kondensatorze sekcji audio za filtrem wejsciowym.
5. Nie wolno prowadzic powrotu pradu glosnika przez mase TEA, PT, wejsc audio ani VREF.

## Zasady prowadzenia sciezek na PCB

### PAM8406
- kondensatory 10 uF i 100 nF przy VDD jak najblizej pinow
- 1 uF VREF bezposrednio przy pinie VREF
- wyjscia glosnikowe prowadzic krotko i symetrycznie
- unikac prowadzenia wejsc audio rownolegle do wyjsc glosnikowych
- nie prowadzic linii SDA i SCL obok wejsc audio PAM

### TEA i PT
- TEA i PT zasilac z osobnych wyjsc LDO
- przy obu ukladach lokalne 100 nF blisko pinow zasilania
- sygnaly audio z TEA do PT i z PT do PAM prowadzic jako krotkie, czyste sciezki
- linie I2C trzymac z dala od wejsc audio i VREF

### Czesc ogolna
- unikac wspolnych waskich gardel masy
- nie przecinac analogowej sekcji powrotem pradu z koncowki mocy
- sekcje PAM trzymac fizycznie dalej od TEA niz wrazliwa sekcje wejsciowa
- glosnik i jego przewody traktowac jako zrodlo zaklocen, wiec nie prowadzic ich nad sekcja wejsciowa

## Sugerowana lista elementow

### Filtr wejsciowy 5 V
- L1: 120 uH, ekranowany, Isat >= 2.5 A
- C1: 470 uF, 10 V lub 16 V, elektrolit

### Galaz PAM8406
- C2: 10 uF X7R
- C3: 100 nF X7R
- C4: 1 uF X7R na VREF
- opcjonalnie C5: 22 uF X7R

### Galaz LDO TEA
- U_LDO_TEA: TPS7A2033
- CIN: 10 uF X7R + 100 nF X7R
- COUT: 10 uF X7R + 100 nF X7R

### Galaz LDO PT
- U_LDO_PT: TPS7A2033
- CIN: 10 uF X7R + 100 nF X7R
- COUT: 10 uF X7R + 100 nF X7R

## Zmiana wzmacniacza z PAM8403 na PAM8406

Powody:
1. PAM8403 ma status NRND i EOL po stronie Diodes.
2. PAM8406 jest aktywnym ukladem.
3. PAM8406 ma lepszy katalogowy SNR.
4. PAM8406 pozwala przelaczac Class D / Class AB.
5. Tryb Class AB moze byc korzystny w projekcie z Flipperem, gdy priorytetem jest redukcja zaklocen EMI i slyszalnych artefaktow.

## Wnioski z EVB User Guide Diodes

Zrodla lokalne w repo:
- [pdf/PAM8406-User-Guide.pdf](pdf/PAM8406-User-Guide.pdf)
- [pdfimages/PAM8406-User-Guide/board_schematic.png](pdfimages/PAM8406-User-Guide/board_schematic.png)
- [pdfimages/PAM8406-User-Guide/page-005.png](pdfimages/PAM8406-User-Guide/page-005.png)
- [pdfimages/PAM8406-User-Guide/page-007.png](pdfimages/PAM8406-User-Guide/page-007.png)

Podglad inline w GitHub Markdown:

![PAM8406 EVB board schematic](pdfimages/PAM8406-User-Guide/board_schematic.png)

![PAM8406 EVB page 005](pdfimages/PAM8406-User-Guide/page-005.png)

![PAM8406 EVB page 007](pdfimages/PAM8406-User-Guide/page-007.png)

Najwazniejsze obserwacje z EVB:
1. Producent stosuje osobne odsprzeganie dla VDD i dla PVDD.
2. Producent stosuje osobny kondensator 1 uF na VREF i podkresla, ze jest to kondensator krytyczny.
3. Producent stosuje ferrytowe filtry EMI na wszystkich czterech liniach wyjsciowych glosnika.
4. Producent prowadzi rozdzielenie masy analogowej i masy mocy, a polaczenie robi w kontrolowanym miejscu.
5. Producent wymaga bardzo bliskiego placementu kondensatorow zasilania przy pinach ukladu.

## BOM referencyjny z EVB producenta

Elementy bezposrednio z EVB User Guide:

- C7, C8: 10 uF, X5R/X7R, ceramic, 0805, 10 V
	Opis: glowne odsprzeganie PVDD i lokalny magazyn energii dla transjentow wyjsciowych.

- C3, C4: 1 uF, X5R/X7R, ceramic, 0603, 10 V
	Opis: szybkie odsprzeganie PVDD dla zaklocen wyzszej czestotliwosci.

- C6: 1 uF, X5R/X7R, ceramic, 0603, 10 V
	Opis: odsprzeganie VDD.

- C5: 1 uF, X5R/X7R, ceramic, 0603, 10 V
	Opis: bypass VREF.

- C1, C2: 1 uF, X5R/X7R, ceramic, 0603, 10 V
	Opis: kondensatory sprzegajace na wejsciu audio.

- R1, R2: 10 kOhm, 0805, 1%
	Opis: rezystory wejsciowe ustawiajace warunki filtru wejsciowego i zgodnosc torow lewy/prawy.

- R4 wedlug BOM EVB: 10 Ohm, 0805, 5%
	Opis: separacja AVCC od PVDD.

Uwaga:
- na wycinku schematu EVB widoczny jest rezystor oznaczony jako R3 przy torze MUTE/VDD,
- a w tekscie BOM pojawia sie R4 = 10 Ohm jako separacja AVCC od PVDD,
- wiec dokumentacja EVB ma tu niespojnosc nazewnicza,
- ale sama idea 10 Ohm pomiedzy domena analogowa i zasilaniem mocy jest wyraznie intencjonalna.

- C13: 220 uF, elektrolit, 10 V
	Opis: glowny kondensator zasilania na wejsciu plytki EVB.

- FB1, FB2, FB3, FB4: ferrite bead, 2 A, 120 Ohm, 0805
	Opis: filtr EMI na wyjsciach glosnikowych.

- C9, C10, C11, C12: 220 pF, X5R/X7R, ceramic, 0603, 10 V
	Opis: kondensatory wspolpracujace z ferrytami na wyjsciu, ograniczajace skladowe wysokiej czestotliwosci.

## Jak to przekladamy na nasz projekt

Elementy, ktore warto skopiowac praktycznie 1:1:
1. C5 = 1 uF na VREF, bardzo blisko pinu.
2. Szybkie odsprzeganie 1 uF przy VDD i przy PVDD.
3. Dodatkowe lokalne 10 uF przy sekcji mocy.
4. Rozdzielenie masy analogowej i masy mocy.

Elementy, ktore adaptujemy do naszej architektury:
1. Zamiast samego C13 = 220 uF na wejsciu robimy mocniejszy filtr systemowy 120 uH + 470 uF.
2. TEA i PT dostaja osobne LDO 3.3 V, czego EVB PAM8406 oczywiscie nie ma.
3. Sterowanie MODE, SHDN i MUTE prowadzimy do GPIO Flippera.

## Czy robic filtr EMI na wyjsciach glosnikowych

Krotka odpowiedz:
- tak, warto przewidziec filtr EMI na PCB,
- ale najlepiej jako sekcje opcjonalna z mozliwoscia DNP, a nie jako bezwzglednie wymagany montaz.

Powod:
1. PAM8406 w Class-D przełącza wyjscia z wysoka czestotliwoscia i to samo w sobie generuje skladniki EMI.
2. Nawet jesli glosniki beda dedykowane i przewody beda krotkie, nadal mozna promieniowac zaklocenia do toru radiowego i do okolicy Flippera.
3. W twoim projekcie i tak walczysz z zakloceniami audio, wiec zostawienie footprintu na filtr EMI jest po prostu rozsadne.
4. W trybie Class-AB filtr EMI na wyjsciu jest mniej krytyczny, ale w trybie Class-D nadal moze byc bardzo przydatny.

Decyzja projektowa dla v1.1:
- footprinty dla FB1, FB2, FB3, FB4 oraz C9, C10, C11, C12 przewidziec na PCB,
- pierwsze prototypy mozna zmontowac w dwoch wariantach:
- wariant A: z ferrytami i kondensatorami EMI,
- wariant B: bez nich albo zworki 0R zamiast ferrytow,
- dopiero po pomiarach i odsluchu zdecydowac finalny stuffing.

Praktyczna rekomendacja:
- dla pierwszej rewizji testowej zostawic ferrite beads osadzone,
- kondensatory 220 pF na wyjsciu rowniez przewidziec,
- jesli okaże sie, ze nie sa potrzebne, latwo je usunac albo oznaczyc jako DNP w finalnym BOM.

## Zalecenia placementu wynikajace z EVB

1. C3, C4, C6, C7, C8 musza byc bardzo blisko ukladu PAM8406.
2. C5 na VREF ma byc maksymalnie blisko pinu 8.
3. Trasa wyjsciowa glosnika ma byc daleko od wejsc audio.
4. Power GND i analog GND nie powinny byc laczone jedna waska linia.
5. Prady z wyjsc glosnikowych maja wracac do system ground tylko po stronie power input.
6. Sygnały wejściowe maja wracac do cichej masy sygnalowej.

## Aktualizacja rekomendacji dla naszego PCB v1.1

Dla naszej plytki proponowany zestaw przy PAM8406 powinien wygladac tak:

- VREF:
	- 1 uF X7R, 0603 lub 0805, bardzo blisko pinu VREF

- VDD analogowe PAM8406:
	- 1 uF X7R, 0603 lub 0805, bardzo blisko pinu VDD

- PVDD mocy PAM8406:
	- 1 uF X7R, 0603 lub 0805, bardzo blisko pinow PVDD
	- 10 uF X7R, 0805 lub 1206, lokalnie przy sekcji mocy

- Wejscie audio:
	- jesli zostajemy przy naszym strojeniu wejscia, nadal mozemy przyjac 1 uF lub 4.7 uF zgodnie z docelowym filtrem wejscia,
	- rezystory wejsciowe powinny miec dobra tolerancje i byc sparowane miedzy kanalami

- Wyjscie audio Class-D:
	- ferrite beads 2 A / 120 Ohm jako punkt startowy,
	- kondensatory 220 pF do masy jako opcjonalny filtr EMI,
	- footprint zachowac niezaleznie od tego, czy finalnie stuffing bedzie pelny

## Co z separacja analogowego VDD od PVDD

EVB sugeruje rezystor 10 Ohm pomiedzy AVCC i PVDD.

W naszym projekcie jest to warte rozwazenia, ale z uwaga:
1. My juz mamy mocniejsza filtracje systemowa przed calym torem audio.
2. Mamy osobne LDO dla TEA i PT, ale sam PAM nadal ma wspolne 5 V dla czesci analogowej i mocy.
3. Dodatkowe 10 Ohm miedzy lokalnym VDD i PVDD PAM8406 moze jeszcze poprawic separacje zaklocen.

Rekomendacja:
- przewidziec footprint na rezystor 10 Ohm albo ferrite bead pomiedzy lokalnym VDD i zasilaniem sekcji analogowej PAM8406,
- w pierwszych testach mozna zaczac od 10 Ohm zgodnie z EVB,
- jesli pojawia sie zbyt duzy spadek lub pogorszenie zachowania dynamicznego, footprint pozwoli latwo zmienic element albo go zwarc.

## Nowe funkcje sprzetowe do wyprowadzenia na GPIO Flippera

Dla PAM8406 nalezy wyprowadzic linie sterujace do GPIO Flippera.

Zalecane sygnaly:
1. PAM_MUTE
2. PAM_SHDN
3. PAM_MODE_ABD

Ujednolicenie nazwy:
- w tym projekcie przyjmujemy nazwe PAM_SHDN
- PAM_SHDN i PAM_EN nie oznaczaja tutaj dwoch roznych funkcji
- to ma byc jeden i ten sam pin sterujacy stanem wlaczenia lub shutdown wzmacniacza
- rozne nazwy wynikaja tylko z roznic w nazewnictwie dokumentacji i opisu logicznego funkcji
- w kodzie, schemacie i PCB uzywamy juz tylko nazwy PAM_SHDN

### Dokladny przydzial GPIO

Piny juz zajete:
- pin 16, PC0, C0: I2C SCL
- pin 15, PC1, C1: I2C SDA

Wolne piny GPIO Flippera do sensownego uzycia:
- pin 2, PA7
- pin 3, PA6
- pin 4, PA4
- pin 5, PB3
- pin 6, PB2
- pin 7, PC3

Proponowany finalny przydzial dla v1.1:
- pin 5, PB3 -> PAM_MUTE
- pin 6, PB2 -> PAM_SHDN
- pin 7, PC3 -> PAM_MODE_ABD
- pin 4, PA4 -> RDS_MPX_ADC

Powod takiego wyboru:
- sa to trzy wolne, sasiednie piny, co upraszcza routing i dokumentacje
- zostawiaja PA7 i PA6 wolne na przyszle funkcje albo debug, a PA4 rezerwujemy na wejscie ADC dla RDS z MPX
- pin 7 lezy blisko GND i 3.3 V na zlaczu Flippera, co jest wygodne dla dodatkowych rezystorow podciagajacych i sekcji sterowania

Aktualizacja dla funkcji RDS z MPX:
- pin 4, PA4, jest rezerwowany jako wejscie ADC dla toru RDS_MPX_ADC
- sygnal MPX z TEA5767 nalezy doprowadzic do PA4 przez analogowy tor dopasowujacy
- wariant startowy: wejscie przez offset DC do zakresu ADC 0 do 3.3 V
- wariant preferowany: bufor i wzmocnienie na MCP6001 z ustawieniem wzmocnienia okolo x4 oraz przesunieciem poziomu DC do polowy zakresu ADC
- szczegolowy plan analog front-end i dekodowania RDS znajduje sie w osobnym pliku [rds_mpx_plan.md](rds_mpx_plan.md)

Zalecenie do hardware:
- wszystkie linie sterujace PAM8406 potraktowac jako sygnaly 3.3 V z Flippera
- nie jest potrzebny translator poziomow dla linii sterujacych PAM8406 przy sterowaniu z Flippera 3.3 V
- datasheet PAM8406 dla VDD = 5.0 V podaje: VIH = 1.4 V oraz VIL = 0.4 V dla wejsc Enable, MUTE i MODE
- oznacza to, ze 3.3 V z GPIO Flippera jest pewnym stanem wysokim, a 0 V jest pewnym stanem niskim
- bezposrednie sterowanie z GPIO Flippera jest poprawne elektrycznie dla tych wejsc logicznych
- domyslne stany po wlaczeniu zasilania ustawic rezystorami, nie polegac tylko na firmware

Parametry logiczne potwierdzone z datasheet PAM8406 przy VDD = 5.0 V:
- Enable Input High Voltage, VIH = 1.4 V
- Enable Input Low Voltage, VIL = 0.4 V
- MUTE Input High Voltage, VIH = 1.4 V
- MUTE Input Low Voltage, VIL = 0.4 V
- MODE Input High Voltage, VIH = 1.4 V
- MODE Input Low Voltage, VIL = 0.4 V

Wniosek praktyczny:
- Flipper GPIO = 3.3 V -> logiczne HIGH dla PAM8406
- Flipper GPIO = 0 V -> logiczne LOW dla PAM8406
- sterowanie bezposrednie jest dopuszczalne
- MUTE aktywne niskim poziomem i ma wewnetrzny pull-up
- SHDN aktywne niskim poziomem i ma wewnetrzny pull-up
- MODE: HIGH = Class-D, LOW = Class-AB, pin nie moze pozostac floating

Zalecane rezystory stanowiace stan domyslny:
- PAM_MUTE: zewnetrzny pulldown do GND, aby po starcie audio bylo bezpiecznie wyciszone
- PAM_SHDN: zewnetrzny pulldown do GND, aby wzmacniacz pozostawal w shutdown do czasu inicjalizacji firmware
- PAM_MODE_ABD: zewnetrzny pulldown do GND, aby domyslny tryb byl Class AB

Proponowane domyslne stany logiczne przy starcie:
- PAM_MUTE = LOW, stan bezpieczny, wyjscie audio wyciszone
- PAM_SHDN = LOW, stan bezpieczny, wzmacniacz w shutdown
- PAM_MODE_ABD = LOW, domyslny tryb Class AB

Proponowane stany po poprawnej inicjalizacji aplikacji:
- PAM_MODE_ABD = LOW dla domyslnego trybu FM Class AB albo HIGH dla Class D, jesli user tak wybierze
- PAM_SHDN = HIGH, wzmacniacz wlaczony
- PAM_MUTE = HIGH, audio odblokowane

Priorytet jesli zabraknie pinow:
1. PAM_MUTE
2. PAM_MODE_ABD
3. PAM_SHDN

## Nowe funkcje aplikacji FM do implementacji

Obecnie aplikacja ma mute po stronie PT:
- stan mute jest uzywany w radio.c
- przelaczenie na klawiszu OK jest w radio.c
- ustawienie Config -> Volume tez robi mute PT w radio.c
- zastosowanie ustawien PT odbywa sie przez radio.c i PT/PT22xx.c

W v1.1 nalezy dodac takze sterowanie PAM8406.

### 1. Wspolny Audio Mute
Funkcja:
- klawisz OK i opcja Volume w Config powinny robic jednoczesnie:
- mute PT
- mute PAM8406

Cel:
- pelne wyciszenie toru audio
- mniejsze ryzyko slyszalnych resztek zaklocen po stronie koncowki mocy

### 2. Amp Power
Nowa opcja konfiguracyjna:
- Amp Power: On / Off

Dzialanie:
- steruje linia PAM_SHDN
- pozwala calkowicie wylaczyc koncowke mocy, gdy nie jest potrzebna
- moze ograniczyc pobor pradu i czesc zaklocen w stanach bez odtwarzania

### 3. Amp Mode
Nowa opcja konfiguracyjna:
- Amp Mode: Class AB / Class D

Zalecenie:
- domyslnie dla radia FM ustawic Class AB
- Class D zostawic jako tryb alternatywny

Cel:
- umozliwic uzytkownikowi wybor miedzy nizszym EMI i czystszym zachowaniem a wyzsza sprawnoscia

Mapowanie do GPIO w aplikacji:
- PAM_MUTE na PB3
- PAM_SHDN na PB2
- PAM_MODE_ABD na PC3

### 4. Sekwencja startu i zatrzymania audio
Przy starcie aplikacji:
1. wlaczyc zasilone bloki TEA i PT
2. ustawic stan PT
3. ustawic tryb PAM8406
4. zwolnic mute PAM

Przy zamknieciu aplikacji:
1. wlaczyc mute PAM
2. wlaczyc mute PT
3. opcjonalnie wylaczyc PAM przez SHDN

Cel:
- mniej trzaskow przy starcie i wyjsciu z aplikacji

## Funkcje swiadomie odrzucone z v1.1

Nie dodajemy:
- Mute On Seek

Powod:
- uzytkownik chce slyszec jak radio przechodzi po zakresie i jak zmienia sie dzwiek podczas szukania

## Podsumowanie zmian v1.1

Najwazniejsze zmiany sprzetowe:
1. PAM8403 zastapiony PAM8406
2. glowny filtr 5 V: 120 uH + 470 uF
3. osobny LDO 3.3 V dla TEA
4. osobny LDO 3.3 V dla PT
5. scisle rozdzielenie masy mocy i masy sygnalowej
6. poprawione lokalne odsprzeganie przy PAM, TEA i PT

Najwazniejsze zmiany funkcjonalne:
1. wspolny mute PT + PAM
2. Amp Power On / Off
3. Amp Mode AB / D
4. poprawna sekwencja startu i wylaczania toru audio

## Sekcja RDS Analog Front-End z MCP6001

### Cel

Wzmocnic skladowa RDS (57 kHz) z wyjscia MPXO TEA5767 przed podaniem na ADC PA4 Flippera.
Bez wzmacniacza sygnal RDS to okolo 6.7 mV, co daje zaledwie 8 LSB na 12-bitowym ADC.
Po wzmocnieniu x6 na MCP6001 z filtrem HP odcinajacym audio uzyskujemy okolo 41 LSB,
co poprawia stosunek sygnalu do szumu kwantyzacji o okolo 14 dB.

### Parametry wyjscia MPXO z datasheetu TEA5767

Parametry zmierzone i katalogowe:
- DC bias VMPXO: 680 do 950 mV, typowo 815 mV
- AC output (mono, delta_f = 22.5 kHz): 60 do 90 mV, typowo 75 mV
- Output resistance Ro: max 500 Ohm
- Sink current Isink: max 30 uA

Skladowa RDS na wyjsciu MPXO:
- typowa dewiacja RDS: +/- 2 kHz
- wzgledem testowej dewiacji 22.5 kHz: 2 / 22.5 = 8.9%
- RDS AC na MPXO: 75 mV x 0.089 = okolo 6.7 mV peak

### Schemat obwodu (ASCII)

Koncepcja:
- C1 zdejmuje DC z TEA
- dzielnik Rb1/Rb2 ustawia DC 1.65 V (srodek zakresu ADC)
- C2+R1 filtruja audio (HPF), zeby ×6 nie clipowalo
- MCP6001 wzmacnia ×6 wokol 1.65 V
- wyjscie 1.65 V +/- wzmocniony sygnal idzie do ADC

```
    TEA5767 pin 25 (MPX out)
         |
       [C1 = 1uF]         Zdejmuje DC 815 mV z TEA
         |
         |
       [C2 = 2.2nF]       HPF: przepuszcza >33 kHz (RDS), odcina audio
         |
         |
       node_Y ----------- MCP6001 pin 3 (V+)
         |
       [R1 = 2.2k]        Dolny rez HPF + sciezka DC bias
         |
       Vbias = 1.65 V     DC offset: srodek zakresu ADC
         |      |
       [Rb1]  [Cb = 100nF]   Cb = AC ground na Vbias
       100k     |
         |     GND
       3.3V
         |
        Vbias
         |
       [Rb2]
       100k
         |
        GND


    Wzmacniacz ×6:

                  +--------[Rf = 10k]--------+
                  |                           |
    MCP6001 pin 4 (V-)              MCP6001 pin 1 (Vout)
                  |                           |
                [Rg = 2k]                ADC PA4
                  |                    (Flipper pin 4)
                Vbias = 1.65 V


    Zasilanie:
    MCP6001 pin 5 (Vdd) ---- 3.3V_TEA ---- [C4 = 100nF] ---- GND
    MCP6001 pin 2 (Vss) ---- GND
```

### Pelen schemat jednolinijkowy

```
TEA pin25 --[C1=1uF]--[C2=2.2nF]-- node_Y -------- MCP6001(V+)
                                      |                  |
                                  [R1=2.2k]          wzmacnia ×6
                                      |                  |
            3.3V --[Rb1=100k]-- Vbias --- [Cb=100nF] --- GND
                                  |
                              [Rb2=100k]
                                  |
                                 GND

                                Vbias
                                  |
                              [Rg=2k]
                                  |
                    Vout ---[Rf=10k]--- MCP6001(V-)
                      |
                  ADC PA4           DC na Vout = 1.65 V
               (Flipper pin 4)     AC na Vout = sygnal × 6
```

### Opis polaczen krok po kroku

1. Sygnal MPX wychodzi z TEA5767 pin 25 (DC bias ~815 mV, AC ~75 mV).

2. C1 = 1 uF zdejmuje DC. Za C1 sygnal jest czyste AC, bez skladowej stalej.

3. C2 = 2.2 nF + R1 = 2.2 kOhm tworza filtr HPF (fc = 33 kHz).
   Odcinaja audio i pilota, przepuszczaja RDS (57 kHz) z -1.3 dB straty.
   Bez tego filtru audio po ×6 clipowaloby na ADC.

4. Dzielnik Rb1/Rb2 (2 x 100 kOhm) ustawia DC offset = 1.65 V na nodzie Vbias.
   R1 laczy node_Y z Vbias, wiec MCP6001 V+ widzi DC = 1.65 V.
   To jest dokladnie srodek zakresu ADC 0 do 3.3 V.

5. MCP6001 wzmacnia ×6 (Av = 1 + Rf/Rg = 1 + 10k/2k).
   Rg laczy V- do Vbias. Rf laczy V- do Vout.
   Feedback wymusza V- = V+, wiec Vout DC = 1.65 V.

6. Na wyjsciu MCP6001: DC = 1.65 V, AC = sygnal RDS × 6.
   To idzie bezposrednio na ADC PA4 (Flipper pin 4).

7. Zasilanie MCP6001: pin 5 do 3.3V_TEA, pin 2 do GND, C4 = 100 nF bypass.

### Dlaczego 100 kOhm a nie 10 kOhm na biasie

Obawy:
- czy 100 kOhm nie spowoduje ze wyjscie bedzie za miekkie i RDS zniknie?

Odpowiedz: NIE, bo Rb1/Rb2 NIE SA w sciezce sygnalowej.

Dowod:
- Rb1 i Rb2 lacza sie do node Vbias, nie do node_Y (wejscie MCP6001)
- miedzy node_Y a Vbias jest R1 = 2.2 kOhm
- Cb = 100 nF przy Vbias ma impedancje 28 Ohm przy 57 kHz
- R1 widzi na swoim dolnym koncu: Rb1 || Rb2 || Cb = 50k || 28 = 28 Ohm
- wiec AC ground na Vbias jest praktycznie idealny, niezaleznie od Rb

Porownanie:
- Rb = 100 kOhm: Rb1||Rb2 = 50k, z Cb(28 Ohm): node Vbias AC = 28 Ohm
- Rb = 10 kOhm: Rb1||Rb2 = 5k, z Cb(28 Ohm): node Vbias AC = 28 Ohm
- IDENTYCZNIE! Cb dominuje, Rb nie ma znaczenia dla AC

Sygnal RDS na node_Y (niezalezny od Rb):
- V_Y = V_TEA x R1 / sqrt(R1^2 + Xc2^2) = V_TEA x 2200 / 2540 = 0.866 x V_TEA
- strata HPF na 57 kHz = -1.3 dB, identycznie dla 10k i 100k biasu

Dodatkowa zaleta 100 kOhm:
- C1 blokuje DC z TEA, wiec Rb nie laduja wyjscia TEA DC
- ale 100 kOhm daje dodatkowe bezpieczenstwo:
  gdyby C1 mialo nieliniowy uplyw, 100k ogranicza prad do 33 uA vs 330 uA z 10k
- mniejsze zageszczenie szumow termicznych: szum 100k jest wiekszy,
  ale przytlumiony przez Cb i R1 nie dociera na wejscie MCP6001

### Obliczenie clipping (worst case)

MPX z TEA5767 przy najsilniejszej stacji, oszacowanie na podstawie oscyloskopu:
- max MPX p-p obserwowany: 800 mV = 400 mV peak

Poszczegolne skladowe MPX (proporcje typowe):
- Audio L+R: 640 mV p-p (80% modulacji)
- Pilot 19 kHz: 80 mV p-p (10%)
- Stereo L-R: 320 mV p-p (40%)
- RDS: 32 mV p-p (4%)

Po HPF (fc=33 kHz, tlumienie na poszczegolnych czestotliwosciach):
- Audio 5 kHz: 640 x 0.15 = 96 mV p-p
- Pilot 19 kHz: 80 x 0.50 = 40 mV p-p
- Stereo 38 kHz: 320 x 0.76 = 243 mV p-p
- RDS 57 kHz: 32 x 0.87 = 28 mV p-p
- Suma peak: okolo 407 mV p-p (worst case koherentny)

Po wzmocnieniu x6:
- Worst case output AC: 407 x 6 = 2442 mV p-p = 2.44 V p-p
- Wokol DC 1.65 V: od 0.43 V do 2.87 V
- Zakres ADC: 0 do 3.3 V
- Margines: 430 mV do raila = 13%
- NIE CLIPUJE nawet na najsilniejszej stacji

Typowa stacja (500 mV MPX p-p):
- output AC: 250 x 0.55 x 6 = 825 mV p-p
- od 1.24 V do 2.06 V
- margines: duzy, ponad 1 V do raila

### Weryfikacja parametrow MCP6001 z datasheetu

Zrodlo: [pdf/MCP6001.pdf](pdf/MCP6001.pdf) (DS20001733L)

Parametry potwierdzone jako zgodne z naszym projektem:
- VDD: 1.8 do 6.0 V — nasz 3.3 V jest w zakresie
- IQ: 100 uA typ — pomijalne obciazenie LDO TEA
- Rail-to-rail I/O: VOH = VDD - 25 mV, VOL = VSS + 25 mV przy RL = 10k
- VCM range: VSS - 0.3 V do VDD + 0.3 V — nasz VCM = 1.65 V jest w zakresie
- Phase margin: 90 stopni typ przy G=+1, stabilny przy G=+6
- Input bias current: 1 pA typ — pomijalne przy naszych rezystancjach
- Bypass cap: datasheet zaleca 0.01 do 0.1 uF w odleglosci 2 mm + 1 uF bulk
  Nasz C4 = 100 nF bezposrednio przy pinach = zgodne

Input stage crossover:
- datasheet podaje ze przy VCM = VDD - 1.1 V nastepuje przelaczenie stopni wejsc
- dla VDD = 3.3 V: crossover = 2.2 V
- nasz VCM = 1.65 V < 2.2 V, wiec pracujemy na jednym stopniu = lepsza liniowsc

Capacitive load:
- datasheet ostrzega o problemach przy CL > 100 pF (G=+1)
- nasz CL to wejscie ADC (~5 pF) + trasa PCB (~10 pF) = okolo 15 pF
- daleko ponizej progu, RISO nie jest potrzebny

### GBW check MCP6001

MCP6001 ma GBW = 1 MHz.
- f_-3dB = GBW / Av = 1 MHz / 6 = 167 kHz
- Gain at 57 kHz: Av(57k) = 6 / sqrt(1 + (57/167)^2) = 6 / 1.056 = 5.68x
- Strata wzgledem idealnych 6x: 5.3% = pomijalna

### Slew rate check

MCP6001 slew rate: 0.6 V/us.
- Worst case (800 mV MPX, po HPF i x6): 2.44 V p-p na 57 kHz
- Wymagany slew rate: pi x 57000 x 2.44 = 0.44 V/us
- Zuzycie SR: 73% — przechodzi, ale niewielki zapas
- Typowa stacja (500 mV MPX): 0.15 V/us = 25% SR, duzy zapas
- Wniosek: przy najsilniejszych stacjach sygnal jest blisko limitu SR,
  ale to dotyczy tylko worst-case koherentnego szczytu (audio+stereo+RDS w fazie)
  W praktyce to sie zdarza rzadko, wiec jest OK

### Poprawa sygnalu RDS na ADC

Bez MCP6001:
- RDS na ADC: 6.7 mV, okolo 8 LSB (0.806 mV/LSB)
- Quantization SNR: 20 x log10(8) = 18 dB

Z MCP6001:
- RDS na ADC: 6.7 x 0.87 (HPF) x 5.68 (gain) = 33 mV, okolo 41 LSB
- Quantization SNR: 20 x log10(41) = 32 dB
- Poprawa: +14 dB, 5x wiecej kwantow

### BOM sekcji RDS front-end

| Ref  | Wartosc   | Obudowa | Funkcja                          |
|------|-----------|---------|----------------------------------|
| U_RDS| MCP6001   | SOT-23-5| bufor i wzmacniacz               |
| C1   | 1 uF      | 0402/0603| coupling cap (istniejacy)       |
| C2   | 2.2 nF    | 0402    | HPF cap                          |
| R1   | 2.2 kOhm  | 0402    | HPF shunt, DC bias path          |
| Rf   | 10 kOhm   | 0402    | feedback resistor                |
| Rg   | 2 kOhm    | 0402    | gain set resistor                |
| Rb1  | 100 kOhm  | 0402    | bias divider gorny               |
| Rb2  | 100 kOhm  | 0402    | bias divider dolny               |
| Cb   | 100 nF    | 0402    | bias node bypass                 |
| C4   | 100 nF    | 0402    | MCP6001 Vdd bypass               |

Lacznie: 1 uklad + 4 kondensatory + 5 rezystorow = 10 elementow.

### Zasilanie MCP6001

MCP6001 zasilany z tego samego LDO TPS7A2033 co TEA5767 (3.3V_TEA).
Kondensator C4 = 100 nF bezposrednio przy pinach Vdd i Vss MCP6001.

Nie potrzeba osobnego LDO dla MCP6001:
- pobor pradu MCP6001: typowo 100 uA
- LDO TEA i tak ma duzy zapas pradowy
- odseparowanie od PAM8406 jest juz zapewnione przez osobny LDO TEA

### Placement na PCB

Zalecenia:
1. MCP6001 umiescic blisko zlacza modulu TEA5767, krotka trasa od C1.
2. C2 i R1 bezposrednio przy wejsciu V+ MCP6001.
3. Rf i Rg bezposrednio przy MCP6001.
4. C4 bypass bezposrednio przy pinach zasilania MCP6001.
5. Rb1, Rb2, Cb moga byc nieco dalej, nie sa krytyczne pod wzgledem trasy sygnalowej.
6. Wyjscie MCP6001 do PA4 prowadzic czysta sciezka, z dala od wyjsc glosnikowych.
7. GND pod MCP6001 ma byc czysta analogowa masa TEA, nie masa mocy PAM.

### Zapasowy wariant wzmocnienia

Jesli na najsilniejszych stacjach wyjscie MCP6001 clipuje:
- zamienic Rg = 2 kOhm na 2.7 kOhm
- nowe wzmocnienie: 1 + 10k/2.7k = 4.7x
- output worst case: 407 x 4.7 = 1.91 V p-p, margines 35%

Jesli sygnal RDS nadal za slaby:
- zamienic Rg = 2 kOhm na 1.5 kOhm
- nowe wzmocnienie: 1 + 10k/1.5k = 7.7x
- GBW check: 1 MHz / 7.7 = 130 kHz, gain at 57k: 7.3x
- output worst case: 407 x 7.7 = 3.13 V p-p, na granicy clippingu

Wniosek: x6 jest dobrym kompromisem. Footprint Rg zostawic z mozliwoscia latwy zamiany.