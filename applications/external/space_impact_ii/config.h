#ifndef CONFIG_H
#define CONFIG_H

/** Grafika **/
#define BACKLIGHT 178, 189, 8 /* Háttérszín RGB kódja */
#define FRAMERATE \
    20 /* Képkocka / másodperc, az eredeti 18, de az nem osztója a 60-nak, így pulldown-t okoz */
#define UPSCALE_FACTOR \
    14 /* Hányszorosára növelje a 84x48-as kijelzõt, 14 ajánlott HD-re, 21 Full HD-re */

/** Hang **/
#define SAMPLE_RATE \
    44100 /* Mintavételezési frekvencia, módosításakor ajánlott a lövéshangot újrakalibrálni egy másik, az algoritmus számára torz frekvenciára */
/*#define LEGACY_AUDIO*/ /* Az eredeti játékban egyszerre csak egy hang szólt, ha ez definiálva van, így lesz */
#define VOLUME \
    4095 /* Egy félkomponens hangereje, a teljes hangerő 32767, maximum 8 komponens szólhat egyszerre, így a maximum hangerő LEGACY_AUDIO nélkül 4095 */

/** Játékelemek **/
/*#define GHOSTING*/ /* A játék néhány verziójában egyszerre csak egy gombot lehetett nyomva tartani, ha ez definiálva van, így lesz */
#define BONUS_COLLIDER   /* A játék néhány verziójában a bónuszobjektum begyűjtötte a lövedékeket pontokért, ha ez definiálva van, így lesz */
/*#define ZEROTH_LIFE*/ /* A játék néhány verziójában az életek száma nem 1 és 3, hanem 0 és 2 között mozgott, ha ez definiálva van, így lesz */
#define LEGACY_TOP_SCORE /* Az eredeti rekordképernyő használata (kikapcsolásával a top 10 pontszám látszik) */
#define PAUSE            /* Ha ez definiálva van, az Esc gomb játék közben megnyomva egy szünet menüt nyit meg */

#endif /* CONFIG_H */
