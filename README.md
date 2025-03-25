# Peer2PeerFileTransporter

## 15. System wymiany plików w architekturze Peer To Peer

### Opis protokołu komunikacyjnego
Do przesyłania plików użyto protokołu warstwy transportowej TCP ze względu na wbudowane mechanizmy retransmisji danych co zapewnia niezawodność w przesyładniu danych nawet przy chwilowej zawodności infrastruktury sieciowej, co idealnie sprawdza się w przypadku przesyłania plików.

Do krótkich komunikatów wymienianych między urządzeniami natomiast wykorzystano protokół UDP, który dzięki m.in. braku wysyłania potwierdzeń otrzymania wiadomości zapewnia dużą prędkość co przydaje się zwłaszcza w przypadku wysyłania wiadomości do wszystkich Peerów w sieci (tzw. Broadcasting).

Odpowiednio wykorzystując oba protokoły stworzyliśmy aplikację umożliwiającą udostępnianie i wymianę plików pomiędzy urządzeniami bez serwera centralnego dzięki wykorzystaniu architektury Peer To Peer (w skrócie P2P).
P2P jest często wykorzystywany np. w grach komputerowych gdyż dzięki zastosowaniu takiej architektury twórcy gry nie muszą tworzyć ani utrzymywać infrastruktury sieciowej (która często jest bardzo kosztowna). W architekturze tej aplikacja jest zarazem klientem jak i serwerem, w architekturze tej każdego użytkownika nazywa się "Peer'em". Dzięki temu Peer'y mogą wymieniać się informacjami bezpośrednio (tzn. bez wykorzystania serwera centralnego).
Architektura ta przypomina tak zwany Mesh (siatkę) w której każdy Peer może nawiązać komunikacje z dowolnym bądź wszystkimi Peerami w sieci. Odłączenie się jednego, dowolnego Peera z sieci nie powoduje braku możliwości przesyłania danych pomiędzy innymi Peerami (dla porównania w przypadku Architektury Klient-Serwer gdy serwer odłączy się od sieci komunikacja nie jest możliwa).
### Opis implementacji

* <details>Jest to plik odpowiedzialny za warstwę wizualną - interfejs graficzny użytkownika.<br />Implementuje on tworzenie okna, dodawnia przycisków oraz list.<br />Wystawia on również funkcje pozwalające implementacji sieciowej na np. aktualizację listy podłączonych Peerów czy wpisywanie komunikatów informacyjnych (celem debugowania).<br />Zawarte są tutaj również akcje które mają zostać wykonane po kliknięciu danego przycisku czy elementu z listy (tzw. Callback'i) - każda z akcji wywołuje odpowiednie funkcje z implementacji sieciowej (jeżeli to konieczne).<summary>WxWidgetsGUI.cpp</summary></details>

* <details>Plik nagłówkowy dla WxWidgetsGUI.cpp zawierający listę metod i pól tej klasy.<summary>WxWidgetsGUI.h</summary></details>

* <details>Plik implementujący funkcje sieciowe architektury P2P, jest odpowiedzialny za wymianę i interpretację komunikatów przesyłanych między Peer'ami, przyjmowanie parametrów z warstwy graficznej (np. do jakiego Peera chcemy się połączyć), przesył oraz zapis udostępnianych plików itd.<br />W celu umożliwienia komunikacji z wieloma Peerami w tym samym czasie, oraz nie blokowania responsywności Interfejsu Graficznego tworzone są nowe wątki gdy jest to potrzebne (Np. przy każdym wysyłaniu pliku).<br />Aktualizacja listy Peerów (przy dołączeniu nowego Peera) polega na wysłaniu aktualnej listy podłączonych peerów do wszystkich przez Peer'a który otrzymał prośbę o dołączenie do sieci. W przypadku odłączania się Peera od sieci, przed rozłączeniem wysyła on do wszystkich wiadomość o odłączeniu po to, aby każdy z peerów usunął go z listy podłączonych peerów.<summary>P2P.cpp</summary></details>

* <details>Plik nagłówkowy dla P2P.cpp zawierający listę metod i pól tej klasy<summary>P2P.h</summary></details>

* <details>Plik zawierający instrukcje kompilacji, automatyzuje pobieranie biblioteki, kompilowanie aplikacji wraz z potrzebnymi bibliotekami.<summary>makefile</summary></details>

### Wymagane biblioteki i sposób kompilacji
Kompilacja wymaga dołączenia biblioteki [WxWidgets](https://www.nuget.org/packages/wxWidgets/3.2.2.1?_src=template).

Za pomocą [Msys2](https://www.msys2.org/) instalujemy MinGW;
```
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-toolchain
```
oraz kompilujemy aplikację.
```
mingw32-make
```
Make pobierze kod źródłowy WxWidgets i skompiluje aplikację (kompilując również wymagane WxWidgets)