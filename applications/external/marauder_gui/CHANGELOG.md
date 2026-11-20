# Changelog

## 1.3

- Add a custom Evil Portal page picker: after a probe-sniff capture, choose between the
  built-in login page template or any `.html`/`.htm` file placed in
  `apps_data/marauder_gui/html/` on the SD card. Falls back to the built-in template if the
  selected file can't be read.
- Rework WiFi Spam: broadcast from a user-supplied `apps_data/marauder_gui/ssids.txt` list
  (one SSID per line) instead of only randomly-generated names. Names are loaded and
  broadcast in safe-sized windows (capped at 500) that rotate every ~20 seconds, so lists
  far larger than the ESP32 Marauder's onboard SSID memory limit can still be cycled through
  continuously. Replaces the old fixed "Rick Roll" entry with a small built-in "Funny Names"
  list that uses the same windowed broadcast path, and removes the separate "Set Count" menu
  item in favor of a single prompt shown when choosing "Random SSID".

## 1.2

- Show EAPOL and beacon counts in PMKID capture, rather than unrelated zero counters.
- Add EAPOL to the existing Data/Auth/Probe breakdown in Raw mode. Show a
  mode-specific counter and total in the other capture modes.
- Mark a recent matching WPA2 M1/M2 candidate as `M1+M2` in the current capture.
  This matches AP/client, replay counter, direction, nonce and MIC presence, and a
  ten-second capture-time window. It does not verify the MIC or recover a password.
  EAPOL alone, including Request Identity, does not set the marker. PMKID-only
  captures and other key descriptor versions are not evaluated by this detector.
  Up to eight recent M1 exchanges are tracked; busy captures can evict candidates.
- Hide capture rows and show an error on a short SD write. Use Back to stop and
  close the file even after seeing `M1+M2`; it is not an automatic stop command.

The live dashboard still attributes packets to BSSIDs: undirected probes with a
broadcast BSSID do not produce an AP row. General counters are not a quality or
password-strength assessment.
