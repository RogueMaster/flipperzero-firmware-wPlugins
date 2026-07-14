The Answer Book turns your Flipper Zero into a book of answers. Hold a question in mind, press OK to open a page, and the book replies with one short answer.

The answer is not a plain random number. Each time you open a page, the app runs a brief, receive-only sensing pass over the radios and sensors around you and folds the result into the answer, so the same question rarely gives the same page twice. The UI never shows a signal scanner; it stays a quiet book of answers.

## Experience

The app has three screens:

- Cover: the title, a closed-book illustration, and a Press OK button.
- Opening: a short page-turn animation while the book reads the environment.
- Page: a numbered archive page showing a rarity sigil, the answer as the hero line, and the answer's category at the foot.

Press OK again on a page to open another. Press Back to close the book.

Each answer belongs to one of five categories (Forward, Observe, Warning, Explore, Reflection) and carries a rarity of Normal, Rare, or Legendary. Legendary pages reveal a special answer.

## How answers are generated

Every page open folds a short sensing pass into a 32-bit hash that seeds the answer:

- BLE signal sampling across advertising channels (skipped if Bluetooth is already active).
- SubGHz signal strength on common regional bands. Receive only, the app never transmits.
- A brief NFC scan for a nearby tag.
- Device state such as time, uptime, and battery level.
- Hardware random samples.

These are reduced to a five-axis vector that biases a weighted category choice; the hash then selects the specific answer and the rarity roll. Because the environment is never identical twice, repeated questions rarely give identical pages.

## Privacy and safety

The app never transmits on SubGHz; all radio use is receive or sample only. BLE sampling is skipped while Bluetooth is already active so it is not disrupted. NFC scanning is short-lived and only runs while a page is opening. It runs on a stock Flipper Zero with no extra hardware required.
