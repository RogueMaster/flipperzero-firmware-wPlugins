#include <stdint.h>
#include "hangman.h"

/*
  Fontname: -Misc-Fixed-Medium-R-SemiCondensed--12-110-75-75-C-60-ISO10646-1
  Copyright: Public domain terminal emulator font.  Share and enjoy.
  Glyphs: 105/4531
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_6x12[1223] =
    "i\0\2\2\3\4\3\5\4\6\14\0\376\7\376\12\376\0\14\1(\1\330 \5\0j\7!\7\71C"
    "g\250\0A\12=Boe=F\266\3B\14=Bg\304*\246Y\305\241\0C\14=Boe\215"
    "\62\312(\247\5D\15=Bg\304*\246\230b\212C\1E\14=B\347F\31\215\224QFCF\15"
    "=B\347F\31\215\224QF\31\1G\14=Boe\215\62\212;-\0H\11=B'\333cd;"
    "I\10\273Bg\305\256\1J\14=Bwg\224QFe\224\0K\13=B'\313T\352\24\253\34L"
    "\16=B'\243\214\62\312(\243\214\206\0M\11=B'\257\225\264;N\12=B'\353\251\222\334:"
    "O\11=Boe\357\264\0P\15=Bg\244\254\207\312(\243\214\0Q\12=BoeW\222Q\5"
    "R\13=Bg\244\254\207*\253\34S\13=Boe\15\67\324i\1T\16=Bg\310\214\62\312("
    "\243\214\42\0U\10=B'\373N\13V\12=B'\273S\231r\4W\11=B'{%\325\5X"
    "\12=B'\353TW\265\16Y\14=B'\353Tg\224QF\21Z\12=Bgh\224\273\321\20a"
    "\12-Bo\303\64t\32\1d\13=B\37e\224\206\266\323\10e\12-Bo\345\241\62\134\0g\13"
    "=:oe;\215\214\322\2h\13=B'\243\214F\312\356\0i\10\273B/#\331\32k\14=B"
    "'\243\214\262L\263\312\1l\7\273BG\366\32n\10-B'\231\332\16o\11-Boe;-\0"
    "p\14=:g\244l\17\225QF\0r\13-B'\231\32e\224\21\0s\12-Boh\270\341P"
    "\0u\10-B';\225\12w\11-B'[Iu\1y\12=:'\333\251n#\0\241\7\71C"
    "'\15\1\361\12EB\317\215\223\251\355\0\0\0\0\4\377\377\4\20\13=Boe=F\266\3\4\21"
    "\16=Bg\244\214\62\32)\353\241\0\4\22\15=Bg\244\254\207\312z(\0\4\23\16=B\347F"
    "\31e\224QF\31\1\4\24\17E>W\246\230b\212)\246\64v\0\4\25\15=B\347F\31\215\224"
    "QFC\4\26\14=B'%U\355\274*U\4\27\15=Bo\345\214\322\206:-\0\4\30\13="
    "B'\233+M\255\3\4\31\15UB'\247\215\263\271\322\324:\4\32\14=B'\313T\352\24\253\34"
    "\4\33\15=Bw\305\24SL\61E\35\4\34\12=B'\257\225\264;\4\35\12=B'\333cd"
    ";\4\36\12=Boe\357\264\0\4\37\10=B\347\366\35\4 \16=Bg\244\254\207\312(\243\214"
    "\0\4!\15=Boe\215\62\312(\247\5\4\42\17=Bg\310\214\62\312(\243\214\42\0\4#\14"
    "=B'\273\323\310(-\0\4$\14=B\67\257J%U\355\10\4%\13=B'\353TW\265\16"
    "\4&\21M:'\246\230b\212)\246\230\206F\31\5\4'\15=B'\333id\224QF\1\4("
    "\17=B'%\225TRI%\225\306\0\4)\21M:'%\225TRI%\225\306F\31\5\4*"
    "\15=BG\243\214\62\232UL\13\4+\14=B'\333S%\225f\0\4,\13\274B'\327+\312"
    "\221\0\4-\15=Bo\345\214\322\310H\247\5\4.\17=B'\246J\245\221TRI&\0\4/"
    "\14=Boh\235FL\245\16\4\60\13-Bo\303\64t\32\1\4\62\13-Bg\244<T\36\12"
    "\4\63\14-B\347F\31e\224\21\0\4\64\14\65>W\246\230bJc\7\4\65\13-Bo\345\241"
    "\62\134\0\4\67\13-Bo\345\250rZ\0\4\70\12-B'\313\225\246\16\4\71\15EB'\247\215"
    "\263\134i\352\0\4:\12\254B'\252$S\31\4;\13-Bw\305\24S\324\1\4>\12-Bo"
    "e;-\0\4\77\10-B\347v\7\4@\15=:g\244l\17\225QF\0\4A\13-Boe"
    "\215rZ\0\4B\14-Bg\310\214\62\312(\2\4C\14=:'\273\323\310(-\0\4E\12-"
    "B'\247\272\252\3\4H\14-B'%\225TRi\14\4K\12-B'\353\251\322\14\4\226\16N"
    ":'\365\347L\375\322\60\303\0\4\242\22U:'\246\230b\32)\246\230b\212\32e\24\4\256\15="
    "B'\353Tg\224QF\21\4\272\14=B'\243\214F\312\356\0\4\330\15=Bo\345\214\306\310:"
    "-\0\4\350\14=Boe=F\326i\1'\23\14=B\37e\224\63j\356\14'\27\13\65B'"
    "\247\272\252\65\2\0";

/*
  Fontname: -Misc-Fixed-Bold-R-SemiCondensed--13-120-75-75-C-60-ISO10646-1
  Copyright: Public domain font.  Share and enjoy.
  Glyphs: 110/1294
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_6x13B[1360] =
    "n\0\3\3\3\4\3\5\4\6\15\0\376\11\376\13\376\0\14\1<\2\2 \5\0n\7!\7JC"
    "\307\223\0A\14NB\227\214\42b:\214\230\4B\15NBGI\242\27\222D\227\13\0C\13NB"
    "\17ED\324\223\204\2D\13NBGI\242\377\313\5\0E\13NB\307!\250X\21*\32F\13N"
    "B\307!\250X\21j\4G\15NB\17ED\324R\42I(\0H\13NB\207\210\323a\304I\0"
    "I\11\314B\7E\244/\4J\13NB\27Q\217$\11\5\0K\16NB\207\210EB\232\221$J"
    "$\1L\10NB\207P\77\32M\13NB\207\210tx\10q\22N\15NB\207\250R\71\34$M"
    "$\1O\13NB\17E\304O\22\12\0P\13NBGE\304t\21j\4Q\13V>\17E\304\247"
    "C\204*R\15NBGE\304t!I\224H\2S\16NB\17ED\224R\205$\11\5\0T\11"
    "NB\207I\250\237\0U\12NB\207\210\77I(\0V\14NB\207\210\223\204\304&\24\1W\12N"
    "B\207\210\323\341\223\0X\16NB\207\210$!\321d$\212\210$Y\14NB\207\210$!\321\204:"
    "\1Z\14NB\207Q&\224)\312\204\6_\6\16>\207\1a\12\66B\17Ur\242L\24d\13N"
    "B\247\26\11\245\304\62Qe\13\66B\17Et\30J)\0g\15F:\317E\323HJ\21I(\0"
    "h\13NB\207P\213dB\342$i\12\314B\217H:\322\205\0k\15NB\207P\213\204\64#I"
    "\224\4l\10\314B\307H\277\20n\12\66B\207D\62!q\22o\12\66B\17E\304IB\1p\15"
    "F:\207D\62!U(\212\212\0r\12\66B\207D\62!j\4s\15\66B\17E$\231JD\22"
    "\12\0u\11\66B\207\210\227\211\2w\14\66B\207\210t\70XB\21\0y\14F:\207\210\313D\221"
    "$\241\0\241\7RC\7\351p\361\15NB\227$E.\221LH\234\4\0\0\0\4\377\377\4\20\15"
    "NB\227\214\42b:\214\230\4\4\21\15NBGE\250X\21\61]\0\4\22\15NBGE\304t"
    "\21\61]\0\4\23\12NB\307!\250\37\1\4\24\14V>\27E\377/\7a\0\4\25\14NB\307"
    "!\250X\21*\32\4\26\15NBGD/'\321E/\1\4\27\17NB\17E$T\232\12I\22"
    "\12\0\4\30\17NB\207\210R\71\34(\25\22I\0\4\31\17VBO,\64\27\261\34*$\222\0"
    "\4\32\17NB\207\210EB\232\221$J$\1\4\33\13NB\27E\377\13\211$\4\34\14NB\207"
    "\210tx\10q\22\4\35\14NB\207\210\323a\304I\0\4\36\14NB\17E\304O\22\12\0\4\37"
    "\12NB\307!\304\237\4\4 \14NBGE\304\351\42T\4\4!\14NB\17ED\324\223\204\2"
    "\4\42\12NB\207I\250\237\0\4#\14NB\207\210'IQ\205\2\4$\17NB\17MF\211\350"
    "%B\223Q\0\4%\17NB\207\210$!\321d$\212\210$\4&\13^:\207D\377\277\30\25\4"
    "'\13NB\207\210\223\244\250\3\4(\26NBG$\62\211L\42\223\310$\62\211L\42\223\310!\0"
    "\4)\27^:G$\62\211L\42\223\310$\62\211L\42\223\310!\250\0\4*\15NB\307l\250H"
    "\222\350\205\2\4+\14NB\207\210\251B\351b\21\4,\14NB\207PcE\304t\1\4-\15N"
    "B\7UQRT\223\220\0\4.\21NBGH\22\251\204\70IB\222\222\4\0\4/\16NB\317"
    "\211IR\242HH$\1\4\60\13\66B\17Ur\242L\12\4\62\14\66BGEt\21\221.\0\4"
    "\63\12\66B\307!\250#\0\4\64\13>>\27E\277\34\204\1\4\65\15\66B\17Et\30\212$\24"
    "\0\4\67\15\66B\17E$\223\222$\24\0\4\70\13\66B\207\210\305B\42\11\4\71\16NBO,"
    "\64\27\261XH$\1\4:\15\66B\207\210\42!I\224H\2\4;\12\66B\27E\277\220\4\4>"
    "\13\66B\17E\304IB\1\4\77\12\66B\307!\304\223\0\4@\14F:GE\304t\21*\2\4"
    "A\14\66B\17EDT\222P\0\4B\13\66B\307E\22\22j\2\4C\15F:\207\210\313D\221"
    "$\241\0\4E\15\66B\207HB\223QD$\1\4H\20\66BG$\62\211L\42\223\310$r\10"
    "\4K\14\66BGlV\231T.\2\4\330\17NB\17E$\24\36FL\22\12\0\4\272\14NB"
    "\207P\307C\210I\0\4\242\15V>\207D\277T\364\313\64\0\4\226\17V>G$\177\251\305*\371"
    "\213\64\0\4\350\16NB\17E\304t\30\61I(\0\4\272\14NB\207P\307C\210I\0\4\242\15"
    "V>\207D\277T\364\313\64\0\4\226\17V>G$\177\251\305*\371\213\64\0\4\256\15NB\207\210"
    "$!\321\204:\1\4\272\14NB\207P\307C\210I\0\4\242\15V>\207D\277T\364\313\64\0\4"
    "\226\17V>G$\177\251\305*\371\213\64\0\0";

void hangman_set_font(Canvas* canvas, const uint8_t h) {
    canvas_set_custom_u8g2_font(canvas, h == 12 ? u8g2_font_6x12 : u8g2_font_6x13B);
}