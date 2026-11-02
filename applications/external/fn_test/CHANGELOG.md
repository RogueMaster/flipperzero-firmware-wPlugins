## v 0.6.2

- The catalog description now explains what a fiscal drive is, which hardware
  and wiring is needed and how to control the app.
- The short description is taken from application.fam only, so the catalog
  bundle is built without warnings.

## v 0.6.1

- Fix the catalog changelog: inline code was removed, because the Flipper
  application catalog does not accept it, so the app can be published there.
- No changes in the app behaviour.

## v 0.6

- Full Russian UI with a built-in Cyrillic font: main menu, waiting screen with
  the FN connection animation, scrollable info screens, MGM erase confirmation
  and result screens.
- Every FN response code from Protocol v1.2.05 has a Russian title and hint;
  unknown codes are shown as «Неизвестный код ответа ФН: 0xNN».
- Reading can be cancelled with Back (≤ 1 s); a late answer is discarded by
  operation id, the next operation works without restarting the app.
- MGM erase is protected twice: «МГМ/боевой» preflight by 33h data and an
  explicit confirmation; the command is sent exactly once and cannot be
  cancelled mid-flight.

## v 0.5

- Fix reading the FN answer: the answer code is masked with 0x7F, the high bit
  is the warning flag. An FN answer with warnings was treated as an error.
- Fix a race in fn_uart_trx(): wait until the RX DMA is armed before sending
  a request, otherwise a fast answer is lost.
- Disable the expansion module detection while the app holds USART (pins 13/14),
  otherwise the port cannot be acquired and the app aborts.
- Keep the USB CLI console enabled: it is independent from USART (pins 13/14),
  so logs stay available via the log command while the app is running. Removed
  the legacy cli_vcp_disable() call and the log-level suppression during
  transactions.

## v 0.4

- API: 1.3.2 support

## v 0.3

- API: 60.4 support (SDK 100)

## v 0.1

- First betta version
