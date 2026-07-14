<!-- Source: https://docs.flipper.net/zero/sub-ghz/frequencies -->
<!-- Mirrored: 2026-07-14 -->

# Frequencies - Flipper Zero Documentation

## Frequencies

The Flipper Zero's Sub-1 GHz module can receive signals across 300–348 MHz, 387–464 MHz, and 779–928 MHz bands. However, transmission is restricted to frequencies permitted for civilian use in your region.

### Determining Your Region and Allowed Frequencies

The send function is disabled by default until the firmware is updated via the Flipper Mobile App or qFlipper with a microSD card inserted. During the update, your region is determined and transmission frequencies are unlocked accordingly.

To check your region code, press and hold the down button while on the desktop.

### Regional Frequency Bands

| Frequency Bands | Regions |
|---|---|
| 433.05–434.79 MHz, 868.15–868.55 MHz | Albania, Algeria, Andorra, Armenia, Azerbaijan, Belarus, Bosnia and Herzegovina, Egypt, European Union, French Polynesia, Georgia, Holy See, Iceland, Jersey, Jordan, Kazakhstan, Kosovo, Kyrgyzstan, Lebanon, Liechtenstein, Libya, Moldova, Montenegro, Morocco, North Macedonia, Norway, Palestine, Réunion, Russia, Serbia, Switzerland, Syria, Turkey, Ukraine, United Kingdom, Uzbekistan |
| 304.10–321.95 MHz, 433.05–434.79 MHz, 915.00–928.00 MHz | Argentina, Australia, Bolivia, Brazil, Canada, Chile, Colombia, Mexico, New Zealand, Peru, Puerto Rico, United States Minor Outlying Islands, United States of America |
| 420.00–440.00 MHz | United Arab Emirates |
| 304.50–321.95 MHz, 433.075–434.775 MHz, 915.00–927.95 MHz | Taiwan |
| 300.00–300.30 MHz, 312.00–316.00 MHz, 433.50–434.79 MHz, 444.40–444.80 MHz | Singapore |
| 433.05–434.79 MHz | Israel |
| 430.00–440.00 MHz | Philippines |
| 433.05–434.79 MHz | India |
| 314.00–316.00 MHz, 430.00–432.00 MHz, 433.05–434.79 MHz | China |
| 312.00–315.25 MHz, 920.50–923.50 MHz | Rest of the world |

### Finding Your Remote's Frequency

Use the frequency analyzer to determine what frequency your remote control operates at.

Attempting to transmit at prohibited frequencies will display: "Transmission is blocked: transmission on this frequency is restricted in your region."
