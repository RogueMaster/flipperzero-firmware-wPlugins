# Internal Help

This is the same help file that lives on the Flipper under Help: a gentle introduction to Morse code, telegraphy, or CW. It points at how to learn, _what_ is worth practising, which resources are worth seeking out, and where to go next once the first noises start becoming letters.

## First steps

- Welcome to Morse Flipper. This is a practical little CW trainer for learning to copy real Morse with your own ears.
- This is the pocket help file. The full manual is on GitHub: https:// github.com/ yo3gnd/ morse-flipper
- Good telegraphy is less about keying and more about learning to copy with that one decoder between your ears.
- Morse is not dead. It is still used in ham radio, prepper circles, and now and then as a bit of drama in film and media.
- There are plenty of software decoders, but there is just one that truly matters: the one between your ears. Train that every day.
- Small daily practice beats a grand weekly binge. Morse code likes repetition, not theatre.
- Perhaps a small orange thing in your pocket can help with that.
- In ham radio, Morse is usually called CW, short for continuous wave. You will hear that term far more often than "Morse". Hams have a long tradition of making plain things sound more technical than they really are.
- This is the only screen where the app will show dits • and dahs —. You will not see them anywhere else. This is by design.
- Think of how you learnt the multiplication table. Nobody sensible counts up to work out 8 times 3; they just know it is 24. Morse should become the same sort of reflex: hear the sound, get the letter. No thinking involved.
- You didn't learn the multiplication table by counting, but by repetition, and you shouldn't learn Morse code by counting.
- This is important: if you catch yourself thinking out in dots and dashes, you are counting. Stop and make the exercise harder. Practise only outside the comfort zone.

## Input & keys

- Real telegraphy always uses a device called a key, manipulator or tapper.
- The Flipper's buttons do work as a key. They are still a fairly rotten excuse for one.
- The classic movie key is called a straight key (SK). You press it down, it closes a contact, and that is the whole trick.
- This is what movies like Lincoln, Titanic, On the Beach, Western Union or the 1899 series show when someone is operating a telegraph.
- With an SK, you control the timing: marks, gaps, all of it.
- No straight key operator is perfectly identical to another. That personal style is called a fist, and experienced operators can often recognise one another by it.
- Do not confuse a fist with bad sending. A good operator still sends cleanly; the differences are just small human ones.
- Hours on a straight key will do your wrist no favours. That is one reason paddles and bugs turned up so early - think Vibroplex - a neat mechanical contraption generating dits with an oscilating pendulum.
- The next keyers were devices with two contacts, one for dots and one for dashes. They are pushed from the side with a precise finger motion.
- This device is called a paddle and needs support electronics. With a paddle and keyer, the code is usually cleaner and more even. That makes it easier on the ears and easier to copy. Most transceivers have these electronics inside them - you just connect a paddle and it works.
- This app supports both SK and paddle inputs, and has multiple keyers available. They can simply be connected to GPIO.
- Most people start without a proper key. That is fine. The Flipper can stand in for a while, but it gets bloody annoying before long, and you will want the real thing.
- Because of how the joystick is made, you cannot sensibly pinch two of its buttons at once. So the app reassigns the Back button.
- If you are using buttons as an input source and you are in paddle mode, Back becomes one paddle and OK becomes the other. You can key, pinch, and feed a real keyer.
- To exit such a screen, long press Left. When Back is being nicked as a paddle input, this symbol appears on the right: ⏴ This is your reminder how to leave the screen.
- It is a compromise solution, but there is sadly no way around it - keyers want two buttons pressed at once.

## Connecting the paddle

- Use a 6.5 mm female jack, solder header pins on it, and plug it straight into the GPIO port. Simple is good here.
- Use this wiring:
  - P3 → ⏚ gnd (sleeve)
  - P5 → dah (ring)
  - P7 → dit (tip)
- With this layout, ground ⏚ sits on P3. That is fine physically. The app should still use P3 as the default virtual ground.
- The jack and cable should point outward, away from the screen and buttons. This is neater, sturdier, and less annoying than loose GPIO wires.
- If your paddle uses a 3.5 mm plug, use a simple 3.5 mm to 6.5 mm adapter, just the usual audio sort.
- You can also solder a suitable 3.5mm TRS jack - the GPIO can be reassigned in settings.
- A straight key fits the same header:
  - P3 → ⏚ gnd
  - P7 → dit / SK
- One adapter covers both cases:
  - Paddle → P3 / P5 / P7
  - Straight key → P3 / P7
- If your wiring does not match this layout, change it from Settings → Keying → GPIO. You can move dit/ SK, dah and the ⏚ ground pin in there. Straight mode always uses the dit/SK pin from that menu. This layout matches the female jack to the GPIO header with the least swaps.
- Regardless of layout, you always need a ground ⏚. P3 can be reassigned if that is more convenient, or you can use any Flipper ground pin. If your key has no ground contact, it will not work.

## How to practice

- You can become perfectly competent at CW and still do most of your sending from a computer. I rarely touch my key unless I have to.
- The website www.LCWO.net is the gold standard for learning to tell letters apart by sound. It starts with two letters.
- Once you stop making mistakes on those two, you add another.
- When you add a new letter, resist the urge to translate it into dots and dashes. Learn the sound directly. This app avoids showing the pattern on purpose, because once you start reading Morse as dots and dashes, you have made the whole thing harder than it should be.
- There is a Listening trainer in here too: same sort of Koch/ LCWO style progression, and the same idea, just on a small orange gadget instead of a laptop.
- Keep the WPM high. You want to recognise letters by sound, not count dits and dahs. Stay above 20 wpm; start at 30, or 25 if 30 is simply too much.
- Do not lower the WPM below 30 before you reach lesson 10.
- If you catch yourself spelling it out as dash dot dash and then deciding it is K, the speed is too low. Turn it up.
- You **really** want to stop thinking out the sound. Remember how you learnt the multiplication table.
- Use Farnsworth to open up the gaps. The spacing should help you hear the rhythm, not tempt you back into counting.
- You need to hear letters, not inventory. If you only copy shapes back as "three dots, one dash", you are practising parroting, not Morse. Doing this will not help you.
- You can also use this app to sharpen straight key sending and practise five character sending groups competitively. Do not make those the main event too early. Copying characters by ear comes first. If you cannot hear the letters reliably, better sending just makes tidier nonsense.
- Straight key sending is a skill of its own. The line between a recognisable personal fist and sloppy sending is subtle, and beginners usually cannot hear the difference yet. The Straight key trainer grades your marks, gaps and dah/dit ratio. Groups of 5 has Easy, Medium and IARU HST scoring. Use Medium as the first serious target and start at 15 WPM. Drop to Easy if the timing feels impossible, but remember that Easy still permits sending that would sound rough on the air.
- On the Straight key trainer result line: The target ratio is dah/dit = 3. OK means that part was close enough. When you see a number, not OK, it is a percentage score. Higher is better. A low number means that your sending was far from the expected timing. Above 80%, you will only see OK.
  - S → spacing
  - di → dit length
  - da → dah length
  - r → dah to dit ratio
- For Groups of 5, Easy is a forgiving warm up. Medium expects clean practice timing and is the sensible everyday target. IARU HST uses competition style scoring. Use it when Medium stops being useful and you want the app to be rude but fair.
- Music practice is oddly useful, especially rhythm work with a metronome. Pitch drills, not so much. Do use a metronome if you want clean skill transfer. You should use a metronome anyway during music practice. Hanons helped my SK transmit speed.

## Prepping

- In prepper and survivalist circles, CW has a certain reputation: harder than voice, but one of the most self sufficient modes left when things go wrong.
- This app works as an emergency CW transmitter, but let us be sensible: it would not be my first choice. Probably not even my second. Flipper's transmitter chip drifts when keying below 25 wpm.
- That drift is a hardware issue, so you do not really fix it in software. A human can tolerate it; cheap decoders will have a fit. It drifts back after pauses, which makes it sound worse still.
- Voice needs a lot of bandwidth. Digital is fussier. CW can get by with a simple trans mitter, a key, and a very small pile of kit.
- The Titanic's operators kept transmitting long after all else failed. Morse was one of the last things still working, and the disaster proved ships needed proper backups with batteries on emergency radio.
- People do in fact build and sell radios for exactly this sort of thing. Small CW QRP rigs, often sold to the prepper crowd, sometimes with decoders built in for when your ears are having an off day.
- Low power CW on HF with a wire in the trees is hard to locate. A voice call on a fixed emergency frequency is not.
- Pair CW with NVIS: a wire aimed almost straight up, bouncing signals off the ionosphere. Roughly 500 to 1500 km, two trees, one wire.
- Sort the plan out now, not during the emergency: time, frequency, fallback. For example: 7.030, daily at 20:00, listen two minutes, then try again tomorrow.
- Your messages will be short. `ALL OK` is two words. `QTH DAMAGE NEED MED FER 3` is six. Plan them like you plan the rest of your kit.
- Use voice for everyday chat. Use CW when power is barely available, conditions are bad, or you want low profile. Digital if you have the gear. Use the simplest thing that works. This is emergency radio, not surgery on nuclear rockets.
- An agreed code can be better still. If both sides know that "SAQ" means QTH DAMAGE NEED MED FER 3, sending SAQ over and over may get through more easily than a full sentence.
- The realistic takeaway is this: CW is worth having but only as a backup skill. Not your first tool, not your main plan, but occasionally the only thing still working.

## A complete Morse contact

- CW has its own little language. Keep it short, assume the other fellow is copying by ear, and do not waste his patience.
- Every extra word is something the other operator has to copy. Cut it down hard and leave the grammar at the door.
- Common abbreviations:
  - FER - for
  - DE - from
  - R - roger, received
  - K - over
  - ES - and
  - GA - good afternoon
  - GM - good morning
  - GE - good evening
  - FB - good (fine business)
  - 73 - bye bye
  - TU, THX - thank you
  - 88 - hugs
  - 99 - usually means 'go away', but can be read as a more vulgar 'fuck off'
- More abbreviations:
  - HW - how
  - SK - logging off
  - NIL - nothing
  - LID - newb
  - BK - break
  - DX - far away, intercontinental
  - CQ - calling everyone
  - CFM - confirmed
  - AGN - again
  - UR RST - your signal is
  - ANT - antenna
  - QSO - 2 way radio contact
- A few more: Often one side sends slash - the old "[Shave and a Haircut](https://en.wikipedia.org/wiki/Shave_and_a_Haircut)" rhythm - and the other replies E E.
  - 5NN - very good signal
  - 3NN - average signal
  - 2NN / 1NN - weak signal
  - = new thought, period
  - E E - two dots right at the end of a contact.
- OM/ YL. The old CW shorthand makes men "old men" and women "young ladies", which gives the game away rather nicely. XYL means wife, as in ex YL. Roughly the same energy as calling your wife your ex girlfriend.
- Here is an absolutely real exchange from my log.
- `CQ CQ CQ DE YO3GND YO3GND K` This is YO3GND calling. Is anyone listening?
- `DE P5KIM K` This is P5KIM answering.
- `RR P5KIM DE YO3GND = GM UR 5NN HW IS KOREA? BK` Roger P5KIM from YO3GND. Good morning, your signal is strong. How is Korea? Back to you.
- `BK GM KOREA FB = NEW ANT NEW ICBM NUKE BK` Good morning. Korea is doing well, I have a new antenna and a new nuclear ballistic missile. Back to you.
- `RR NEAT TOYS = TU FER QSO ES 73` Roger. Interesting gear you have there. Thank you for the contact, and goodbye.
- `RR TU 73 E E` Roger. Thank you and goodbye.
- At that point the contact between P5KIM and YO3GND is complete.
- Some people are chatty and send full phrases. That will be slow. Do not expect it as the default.
- A contact (a qso) can be even shorter than that. Just callsigns and signal reports still count perfectly well.

## Contesting

- Telegraphy is still very much alive as a performance sport in the ham bands.
- Contests are yearly events when the world's radio amateurs all pile onto the bands for a weekend and try to work one another.
- During a contest weekend, CW gets everywhere. Tune about a bit and you will hear TEST - short for contest - until it starts haunting your sleep.
- Contesting rewards practice, but anyone can have a go.
- Some operators might slow down for you. It is considered polite. It is also not something to count on during a contest.
- Unlike the previous P5KIM contact, contest exchanges are usually very quick. Here is the standard contest exchange:
- `YO3GND TEST` `P5KIM` `P5KIM 599 23` `599 37` `TU`
- That means: 1. I am YO3GND in the current contest. 2. I am P5KIM, also in the contest. 3. Put number 23 in your log. 4. Roger, put 37 in yours for me. 5. Thanks, goodbye
- Anything else except this standard exchange is not expected in a contest. Do not do this, unless you personally know the other fellow and you are sure he does not mind. Certainly do not do it with strangers.
- The receiver is expected to know which contest is happening. Nobody explains it while it is ongoing and most certainly not in Morse code.
- Contests do not allow duplicate contacts. Some penalise them. If you try one, you will receive NIL or DUPE. Keep at it and they may send LID, 99 or ignore you.
- Contesting rarely happens with a paddle and paper. Most people use logging software such as N1MM to log, check dupes and automate the boring bits.
- Contesting can be done with a paddle, but most high speed operators send by keyboard and macros.
- They still decode with their own brain, and that is exactly what this app is trying to train.
- Contesting with a straight key is very rare. I know just one person who does it.
- Contesting tends to happen around 25-30 wpm. Expect 30+ early on.

## USB & live practice

- There are websites like iCW, [vail.woozle.net](http://vail.woozle.net/), [morse.halb.it](https://morse.halb.it/), and VBand that let you practise with other people using a real paddle.
- To do this, you need a computer adapter for your SK or paddle. This app can emulate an adapter for CW practice sites. You can still use the Flipper's buttons if you must.
- Most simple adapters lie to the computer and pretend to be a keyboard or mouse. That is how they get a key or paddle in. The fancy word for doing this is emulation.
- The site reads those key presses, generates audio for your speakers, and sends your keypresses, as Morse, to the other participants.
- The nicer modern option is MIDI: lower latency, quicker response, and better timing between the browser and the adapter.
- A MIDI adapter lets you keep that Vail browser tab in the background and do other things with your browser.
- This app supports a Vail style MIDI adapter properly, including keyers, WPM and tone control.
- You do not need a callsign to practise on those sites, nor a transmitter. The experience is similar to a real QSO and worth it - especially if you are still afraid of your first live transmission on the bands.
- CW has its own language. If you want to operate on those sites, you will be expected to understand it. Read "A complete Morse contact" to understand it.

## Ham usage

- Ham Keyer is a field keyer and logger in one slightly opinionated box. It can send fixed lines like RR UR 5NN BK or POTA RO 0038 K, while also keying and logging what you send on the paddles - usually a callsign which cannot be made into a macro.
- With Break-In (BKIN) off, you can still key and it will be decoded and logged, but you will not transmit - keying the transceiver is disabled. Flipper will generate sidetone as you key - great for taking notes in the POTA log itself, or typing a call you misheard.
- With Break-In on (BKIN on) you transmit and it logs. Handy for POTA or SOTA, where the operator already has enough plates wobbling. Logs are dated on the SD card, and each change is timestamped.
- Rig keying uses P15 for key and P16 for PTT. Do not wire a radio on optimism. Check the rig input, polarity, and ground arrangement. Use a transistor or optocoupler if the noise or RFI makes Flipper misbehave.
- Back toggles BKIN. Long press Left exits. With BKIN off, paddle input is logged but not sent to the rig.
- Assign short messages to Up, Down, Left, Right, and OK. Use them for field exchanges, not essays. If logging is enabled, entries go to /ext/apps_data/morse_flipper/morse- flipper- ham- keyer- YYYY MM DD.txt. You get one file each day.

## Troubleshooting

- If keying does not work, start simple: use the Flipper joystick OK as a straight key. Then try OK + Back as button paddles. Then plug in a real key. Settings → Keying is always source plus mode: where the signal comes from, and how the app turns it into Morse.
- A straight key uses dit/SK. A paddle needs dit, dah, and ⏚ ground. The default jack wiring is:
  - P3 → ⏚ ground
  - P5 → dah
  - P7 → dit or SK
- The paddles can be swapped from settings, if needed. When Back is not exiting, it is being used as a paddle. You will see this symbol on the right: ⏴ This is your reminder to long press Left for back.
- A GPIO short warning means the jack wiring looks wrong for the chosen input. Unplug the key and check it. A mono straight key in the paddle jack will be treated as a straight key.
- For USB output, choose Keyboard, Mouse, or MIDI, then wait a bit for the computer to reconnect with the new settings. Custom character sets and ham logs live on the SD card under /ext/apps_data/morse_flipper.
- High quality audio is output only on P2. Connect it to an audio jack. It will sound better if you can add a small filtering capacitor, 1-50µF, between P2 and ⏚ ground. You don't have to use the P2 output; you can always fall back to the internal buzzer for the sidetone.

## Moving forward

- This is only a small app, really, just a practical way into CW. I made it for people who are curious about Morse and RF and would rather prod the thing than read about it for ages.
- Seriously, get a telegraphy key; I suggest paddles over a straight key. The Flipper's silicone membrane buttons will eventually wear out, especially when a practice plateau tempts you to press harder, as if force were the missing skill. Keying from the joystick gets annoying quickly too.
- lcwo.net is the easy daily practice route on a computer. It lets you type what you hear, which means you can practise and make mistakes faster than you can on the Flipper.
- For desktop, Morse Runner Community Edition will simulate live contest traffic, exchanges and general unpleasantness. Look for the Community Edition, not the older one. It is found on github.
- RUFZxp trains upper end decoding speed. It is THE official software in the CW IARU HST World Championship. Yes, CW olympiads are a real thing. IARU HST deals with Morse in theory. For actual contesting in CW, WRTC runs every four years. WRTC 2026 is set to run in Great Britain, and it is more like the Olympic Games.
- http:// vail.woozle.net works nicely with this app in USB MIDI mode, and lets you practise live contacts with other people. Some might even send you a QSL card.
- It is worth trying the exam for a ham licence. In much of Europe it costs almost nothing, and plenty of clubs are happy to help if you turn up actually wanting to learn CW.
- Being licensed on HF means that, at times, you can reach the world with five watts. With a modest hundred watt rig, CW gets surprisingly far surprisingly often.
- Find the Twente WebSDR. [Tune around 14.000-14.070](http://websdr.ewi.utwente.nl:8901/?tune=14044cw) MHz and 7.000-7.050 MHz. On a weekend or during a contest, you will hear plenty of live CW.
- Try CW Academy if you want something more organised than poking at apps and web sites on your own. This app can help, but the course is not casual. They expect a real key, regular practice, and a fair bit of homework.
- The CWA course material is free, and you can also work through it yourself at your own pace.
