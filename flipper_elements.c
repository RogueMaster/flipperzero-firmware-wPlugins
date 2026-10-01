#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <gui/modules/dialog_ex.h> 
#include <gui/view.h>
#include <stdio.h>
#include <string.h>

#include "assets/lab66_logo_52x48.h"

#define VIEW_SPLASH 0
#define VIEW_GRID 1
#define VIEW_DETAILS 2
#define VIEW_ABOUT 3 
#define VIEW_EXIT_PROMPT 4 
#define VIEW_EXIT_SPLASH 5
#define VIEW_REFS 6
#define VIEW_THANKS 7

#define EVENT_START_APP 101 
#define EVENT_SHOW_DETAILS 102
#define EVENT_SHOW_ABOUT 103
#define EVENT_FINAL_CLOSE 104
#define EVENT_SHOW_REFS 105
#define EVENT_SHOW_THANKS 106

#define E(x, y, z, p, g, sym, n, m, mp, bp, d, c, e, af, pl, ie, di, bk, ox, ra, rc, ri, rm, mag, cr, ca, mh, ab, is, po, tc, sh) \
    { x, y, z, p, g, sym, n, m, mp, bp, d, c, e, af, pl, ie, di, bk, ox, ra, rc, ri, rm, mag, cr, ca, mh, ab, is, po, tc, sh }

typedef struct {
    uint8_t col, row, z, period, group;
    const char* symbol; const char* name; const char* mass; const char* melt; 
    const char* boil; const char* dens; const char* cat; const char* e_conf; 
    const char* aff; const char* pauling; const char* ie; const char* disc; 
    const char* blk; const char* ox; const char* r_a; const char* r_c; 
    const char* r_i; const char* r_m; const char* mag; const char* cryst; const char* cas;  
    const char* mohs; const char* abund; const char* iso;
    const char* polar; const char* tcond; const char* sheat;
} Element;

const Element DB[] = {
E(0,0,1,1,1,"H","Hydrogen","1.008","13.9","20.2","8.9e-5","diatomic nonmetal","1s1","72.8","2.2","1312","H. Cavendish","s","-1, 1","25","32","139.9 (H-)","N/A","diamagnetic","mol. hexagonal\n close-packed","12385-13-6","N/A","1400","7 (Stbl: 2)\nNAT (>=1%):\n 1H - 99.9%\nMAX LIFE:\n 3H (12.3y, B-)","4.50","0.1805","14304 [H2(g)]"),
E(17,0,2,1,18,"He","Helium","4.003","0.9","4.2","1.8e-4","noble gas","1s2","-48.0","N/A","2372, 5250","P. Janssen","s","N/A","N/A","28","N/A","N/A","diamagnetic","hexagonal\n close-packed","7440-59-7","N/A","0.008","9 (Stbl: 2)\nNAT (>=1%):\n 4He - 100%\nMAX LIFE:\n 6He (806ms, B-)","1.38","0.1513","5193 [He(g)]"),
E(0,1,3,2,1,"Li","Lithium","6.94","453.6","1603","0.534","alkali metal","[He] 2s1","59.6","0.98","520, 7298","J. Arfwedson","s","1","145","133","76.0 (Li+)","152","paramagnetic","body-centred\n cubic","7439-93-2","0.6","20","11 (Stbl: 2)\nNAT (>=1%):\n 7Li - 95.1%\n 6Li - 4.8%\nMAX LIFE:\n 8Li (838ms, B-)","164.1","85.0","3582 [Li(s)]"),
E(1,1,4,2,2,"Be","Beryllium","9.012","1560","2742","1.85","alkaline earth","[He] 2s2","-48.0","1.57","899, 1757","L. Vauquelin","s","2","105","102","45.0 (Be2+)","112","diamagnetic","hexagonal\n close-packed","7440-41-7","5.5","2.8","12 (Stbl: 1)\nNAT (>=1%):\n 9Be - 100%\nMAX LIFE:\n 10Be (1.38My, B-)","37.7","190.0","1825 [Be(s)]"),
E(12,1,5,2,13,"B","Boron","10.81","2349","4200","2.34","metalloid","[He] 2s2 2p1","27.0","2.04","800, 2427","J. Gay-Lussac","p","3","85","85","27.0 (B3+)","85","diamagnetic","rhombohedral","7440-42-8","9.5","10","16 (Stbl: 2)\nNAT (>=1%):\n 11B - 80.3%\n 10B - 19.6%\nMAX LIFE:\n 8B (771ms, B+)","20.5","27.0","1026 [B(s)]"),
E(13,1,6,2,14,"C","Carbon","12.01","N/A","N/A","2.267","poly nonmetal","[He] 2s2 2p2","121.8","2.55","1086, 2352","Ancient Egypt","p","-4, 4","70","75","16.0 (C4+)","N/A","diamagnetic","hex. graphite","7440-44-0","1.0","200","15 (Stbl: 2)\nNAT (>=1%):\n 12C - 98.9%\n 13C - 1.06%\nMAX LIFE:\n 14C (5.7ky, B-)","11.3","140.0","709 [graphite]"),
E(14,1,7,2,15,"N","Nitrogen","14.00","63.1","77.3","1.2e-3","dia nonmetal","[He] 2s2 2p3","-6.8","3.04","1402, 2856","D. Rutherford","p","-3, 3, 5","65","71","146.0 (N3-)","N/A","diamagnetic","molecular\n cubic","7727-37-9","N/A","19","16 (Stbl: 2)\nNAT (>=1%):\n 14N - 99.6%\nMAX LIFE:\n 13N (9.9m, B+)","7.4","0.0258","1040 [N2(g)]"),
E(15,1,8,2,16,"O","Oxygen","15.99","54.3","90.1","1.4e-3","dia nonmetal","[He] 2s2 2p4","141.0","3.44","1313, 3388","C. Scheele","p","-2","60","63","140.0 (O2-)","N/A","paramagnetic","mol. monoclin.","7782-44-7","N/A","4.61e5","18 (Stbl: 3)\nNAT (>=1%):\n 16O - 99.7%\nMAX LIFE:\n 15O (122s, B+)","5.3","0.0265","918 [O2(g)]"),
E(16,1,9,2,17,"F","Fluorine","18.99","53.4","85.0","1.7e-3","dia nonmetal","[He] 2s2 2p5","328.1","3.98","1681, 3374","A. Ampere","p","-1","50","64","133.0 (F-)","N/A","diamagnetic","mol. monoclin.","7782-41-4","N/A","585","19 (Stbl: 1)\nNAT (>=1%):\n 19F - 100%\nMAX LIFE:\n 18F (109m, B+)","3.74","0.0277","824 [F2(g)]"),
E(17,1,10,2,18,"Ne","Neon","20.18","24.5","27.1","9.0e-4","noble gas","[He] 2s2 2p6","-116","N/A","2080, 3952","M. Travers","p","N/A","N/A","67","N/A","N/A","diamagnetic","face-centred\n cubic","7440-01-9","N/A","0.005","20 (Stbl: 3)\nNAT (>=1%):\n 20Ne - 90.4%\n 22Ne - 9.2%\nMAX LIFE:\n 24Ne (3.38m, B-)","2.66","0.0491","1030 [Ne(g)]"),
E(0,2,11,3,1,"Na","Sodium","22.98","370.9","1156","0.968","alkali metal","[Ne] 3s1","52.8","0.93","495, 4562","H. Davy","s","1","180","155","102.0 (Na+)","186","paramagnetic","body-centred\n cubic","7440-23-5","0.5","23600","20 (Stbl: 1)\nNAT (>=1%):\n 23Na - 100%\nMAX LIFE:\n 22Na (2.6y, B+)","162.7","142.0","1228 [Na(s)]"),
E(1,2,12,3,2,"Mg","Magnesium","24.30","923","1363","1.738","alkaline earth","[Ne] 3s2","-40.0","1.31","737, 1450","J. Black","s","2","150","139","72.0 (Mg2+)","160","paramagnetic","hexagonal\n close-packed","7439-95-4","2.5","23300","22 (Stbl: 3)\nNAT (>=1%):\n 24Mg - 78.9%\n 26Mg - 11.0%\n 25Mg - 10.0%\nMAX LIFE:\n 28Mg (20.9h, B-)","71.2","156.0","1023 [Mg(s)]"),
E(12,2,13,3,13,"Al","Aluminium","26.98","933.4","2743","2.7","post-transition","[Ne] 3s2 3p1","41.7","1.61","577, 1816","H. Davy","p","3","125","126","53.5 (Al3+)","143","paramagnetic","face-centred\n cubic","7429-90-5","2.75","82300","23 (Stbl: 1)\nNAT (>=1%):\n 27Al - 100%\nMAX LIFE:\n 26Al (717ky, B+)","57.8","237.0","897 [Al(s)]"),
E(13,2,14,3,14,"Si","Silicon","28.08","1687","3538","2.33","metalloid","[Ne] 3s2 3p2","134.0","1.90","786, 1577","J. Berzelius","p","-4, 4","110","116","40.0 (Si4+)","117","diamagnetic","diamond cubic","7440-21-3","6.5","2.82e5","23 (Stbl: 3)\nNAT (>=1%):\n 28Si - 92.2%\n 29Si - 4.6%\n 30Si - 3.0%\nMAX LIFE:\n 32Si (157y, B-)","37.3","149.0","703 [Si(s)]"),
E(14,2,15,3,15,"P","Phosphorus","30.97","N/A","N/A","1.823","poly nonmetal","[Ne] 3s2 3p3","72.0","2.19","1011, 1907","H. Brand","p","-3, 3, 5","100","111","212.0 (P3-)","110","diamagnetic","orthorhombic","7723-14-0","0.5","1050","23 (Stbl: 1)\nNAT (>=1%):\n 31P - 100%\nMAX LIFE:\n 33P (25.3d, B-)","25.0","0.236","686 [P(s)]"),
E(15,2,16,3,16,"S","Sulfur","32.06","388.3","717.8","2.08","poly nonmetal","[Ne] 3s2 3p4","200.4","2.58","999, 2252","Ancient china","p","-2, 2, 4, 6","100","103","184.0 (S2-)","104","diamagnetic","orthorhombic","7704-34-9","2.0","350","24 (Stbl: 4)\nNAT (>=1%):\n 32S - 94.8%\n 34S - 4.3%\nMAX LIFE:\n 35S (87.3d, B-)","19.4","0.205","736 [S(s)]"),
E(16,2,17,3,17,"Cl","Chlorine","35.45","171.6","239.1","3.2e-3","dia nonmetal","[Ne] 3s2 3p5","348.5","3.16","1251, 2298","C. Scheele","p","-1, 1, 3, 5, 7","100","99","181.0 (Cl-)","99","diamagnetic","molecular\n orthorhombic","7782-50-5","N/A","145","24 (Stbl: 2)\nNAT (>=1%):\n 35Cl - 75.8%\n 37Cl - 24.2%\nMAX LIFE:\n 36Cl (301ky, B-)","14.6","0.0089","479 [Cl2(g)]"),
E(17,2,18,3,18,"Ar","Argon","39.94","83.8","87.3","1.7e-3","noble gas","[Ne] 3s2 3p6","-96.0","N/A","1520, 2665","Lord Rayleigh","p","N/A","N/A","96","N/A","N/A","diamagnetic","face-centred\n cubic","7440-37-1","N/A","3.5","24 (Stbl: 3)\nNAT (>=1%):\n 40Ar - 99.6%\nMAX LIFE:\n 39Ar (268y, B-)","11.08","0.0177","520 [Ar(g)]"),
E(0,3,19,4,1,"K","Potassium","39.09","336.7","1032","0.89","alkali metal","[Ar] 4s1","48.3","0.82","418, 3052","H. Davy","s","1","220","196","138.0 (K+)","227","paramagnetic","body-centred\n cubic","7440-09-7","0.4","20900","25 (Stbl: 2)\nNAT (>=1%):\n 39K - 93.2%\n 41K - 6.7%\nMAX LIFE:\n 40K (1.24Gy, B-/+)","289.7","102.5","757.8 [K(s)]"),
E(1,3,20,4,2,"Ca","Calcium","40.07","1115","1757","1.55","alkaline earth","[Ar] 4s2","2.37","1.00","589, 1145","H. Davy","s","2","180","171","100.0 (Ca2+)","197","paramagnetic","face-centred\n cubic","7440-70-2","1.75","41500","26 (Stbl: 5)\nNAT (>=1%):\n 40Ca - 96.9%\n 44Ca - 2.0%\nMAX LIFE:\n 48Ca (56Ey, 2B-)","160.8","201.0","647 [Ca(s)]"),
E(2,3,21,4,3,"Sc","Scandium","44.95","1814","3109","2.985","transition metal","[Ar] 3d1 4s2","18.0","1.36","633, 1235","L. Nilson","d","3","160","148","74.5 (Sc3+)","162","paramagnetic","hexagonal\n close-packed","7440-20-2","N/A","22","26 (Stbl: 1)\nNAT (>=1%):\n 45Sc - 100%\nMAX LIFE:\n 46Sc (83.7d, B-)","97.0","15.8","568 [Sc(s)]"),
E(3,3,22,4,4,"Ti","Titanium","47.86","1941","3560","4.506","transition metal","[Ar] 3d2 4s2","7.28","1.54","658, 1309","W. Gregor","d","4","140","136","60.5 (Ti4+)","147","paramagnetic","hexagonal\n close-packed","7440-32-6","6.0","5650","28 (Stbl: 5)\nNAT (>=1%):\n 48Ti - 73.7%\n 46Ti - 8.2%\n 47Ti - 7.4%\n 49Ti - 5.4%\n 50Ti - 5.2%\nMAX LIFE:\n 44Ti (59y, EC)","87.0","21.9","524 [Ti(s)]"),
E(4,3,23,4,5,"V","Vanadium","50.94","2183","3680","6.11","transition metal","[Ar] 3d3 4s2","50.9","1.63","650, 1414","A. del Rio","d","5","135","134","54.0 (V5+)","134","paramagnetic","body-centred\n cubic","7440-62-2","7.0","120","27 (Stbl: 1)\nNAT (>=1%):\n 51V - 99.7%\nMAX LIFE:\n 50V (271Py, B+, B-)","87.0","30.7","489 [V(s)]"),
E(5,3,24,4,6,"Cr","Chromium","51.99","2180","2944","7.15","transition metal","[Ar] 3d5 4s1","65.2","1.66","652, 1590","L. Vauquelin","d","3, 6","140","122","61.5 (Cr3+)","128","antiferromag.","body-centred\n cubic","7440-47-3","8.5","102","28 (Stbl: 3)\nNAT (>=1%):\n 52Cr - 83.7%\n 53Cr - 9.5%\n 50Cr - 4.3%\n 54Cr - 2.4%\nMAX LIFE:\n 50Cr (1.3Ey, B)","83.0","93.9","449 [Cr(s)]"),
E(6,3,25,4,7,"Mn","Manganese","54.93","1519","2334","7.21","transition metal","[Ar] 3d5 4s2","-50.0","1.55","717, 1509","T. Bergman","d","2, 4, 7","140","119","83.0 (Mn2+)","127","complex order","complex cubic","7439-96-5","6.0","950","27 (Stbl: 1)\nNAT (>=1%):\n 55Mn - 100%\nMAX LIFE:\n 53Mn (3.7My, EC)","68.0","7.81","479.5 [Mn(s)]"),
E(7,3,26,4,8,"Fe","Iron","55.84","1811","3134","7.86","transition metal","[Ar] 3d6 4s2","14.7","1.83","762, 1561","5000 BC","d","2, 3","140","116","64.5 (Fe3+)","126","ferromagnetic","body-centred\n cubic","7439-89-6","4.0","56300","28 (Stbl: 4)\nNAT (>=1%):\n 56Fe - 91.7%\n 54Fe - 5.8%\n 57Fe - 2.1%\nMAX LIFE:\n 60Fe (2.6My, B-)","62.0","80.4","449 [Fe(s)]"),
E(8,3,27,4,9,"Co","Cobalt","58.93","1768","3200","8.9","transition metal","[Ar] 3d7 4s2","63.8","1.88","760, 1648","G. Brandt","d","2, 3","135","111","61.0 (Co3+)","125","ferromagnetic","hexagonal\n close-packed","7440-48-4","5.0","25","28 (Stbl: 1)\nNAT (>=1%):\n 59Co - 100%\nMAX LIFE:\n 60Co (5.2y, B-)","55.0","100.0","421 [Co(s)]"),
E(9,3,28,4,10,"Ni","Nickel","58.69","1728","3003","8.908","transition metal","[Ar] 3d8 4s2","111.6","1.91","737, 1753","A. Cronstedt","d","2","135","110","69.0 (Ni2+)","124","ferromagnetic","face-centred\n cubic","7440-02-0","4.0","84","31 (Stbl: 5)\nNAT (>=1%):\n 58Ni - 68.0%\n 60Ni - 26.2%\n 62Ni - 3.6%\n 61Ni - 1.1%\nMAX LIFE:\n 59Ni (81ky, B+)","49.0","90.9","444 [Ni(s)]"),
E(10,3,29,4,11,"Cu","Copper","63.54","1357","2835","8.96","transition metal","[Ar] 3d10 4s1","119.2","1.90","745, 1957","Middle East","d","2","135","112","73.0 (Cu2+)","128","diamagnetic","face-centred\n cubic","7440-50-8","3.0","60","29 (Stbl: 2)\nNAT (>=1%):\n 63Cu - 69.1%\n 65Cu - 30.8%\nMAX LIFE:\n 67Cu (61h, B-)","46.5","401.0","385 [Cu(s)]"),
E(11,3,30,4,12,"Zn","Zinc","65.38","692.6","1180","7.14","transition metal","[Ar] 3d10 4s2","-58.0","1.65","906, 1733","India","d","2","135","118","74.0 (Zn2+)","134","diamagnetic","hexagonal\n close-packed","7440-66-6","2.5","70","30 (Stbl: 5)\nNAT (>=1%):\n 64Zn - 49.1%\n 66Zn - 27.7%\n 68Zn - 18.4%\n 67Zn - 4.0%\nMAX LIFE:\n 65Zn (243d, B+)","38.6","116.0","388 [Zn(s)]"),
E(12,3,31,4,13,"Ga","Gallium","69.72","302.9","2673","5.91","post-transition","[Ar] 3d10 4s2 4p1","41.0","1.81","578, 1979","L. Boisbaudran","p","3","130","124","62.0 (Ga3+)","135","diamagnetic","orthorhombic","7440-55-3","1.5","19","30 (Stbl: 2)\nNAT (>=1%):\n 69Ga - 60.1%\n 71Ga - 39.8%\nMAX LIFE:\n 67Ga (3.26d, EC)","50.0","40.6","371 [Ga(s)]"),
E(13,3,32,4,14,"Ge","Germanium","72.63","1211","3106","5.323","metalloid","[Ar] 3d10 4s2 4p2","118.9","2.01","762, 1537","C. Winkler","p","-4, 4","125","121","53.0 (Ge4+)","122","diamagnetic","diamond cubic","7440-56-4","6.0","1.5","32 (Stbl: 4)\nNAT (>=1%):\n 74Ge - 36.5%\n 72Ge - 27.4%\n 70Ge - 20.5%\n 73Ge - 7.8%\n 76Ge - 7.7%\nMAX LIFE:\n 76Ge (1.88Zy, 2B-)","40.0","60.2","320 [Ge(s)]"),
E(14,3,33,4,15,"As","Arsenic","74.92","N/A","N/A","5.727","metalloid","[Ar] 3d10 4s2 4p3","77.6","2.18","947, 1798","Bronze Age","p","-3, 3, 5","115","121","58.0 (As3+)","121","diamagnetic","rhombohedral","7440-38-2","3.5","1.8","31 (Stbl: 1)\nNAT (>=1%):\n 75As - 100%\nMAX LIFE:\n 73As (80.3d, EC)","30.0","50.2","329 [As(s)]"),
E(15,3,34,4,16,"Se","Selenium","78.97","494.0","958","4.81","poly nonmetal","[Ar] 3d10 4s2 4p4","194.9","2.55","941, 2045","J. Berzelius","p","-2, 2, 4, 6","115","116","198.0 (Se2-)","117","diamagnetic","trig. helical\n chains","7782-49-2","2.0","0.05","33 (Stbl: 5)\nNAT (>=1%):\n 80Se - 49.8%\n 78Se - 23.6%\n 76Se - 9.2%\n 82Se - 8.8%\n 77Se - 7.6%\nMAX LIFE:\n 82Se (87Ey, 2B-)","28.9","0.52","321 [Se(s)]"),
E(16,3,35,4,17,"Br","Bromine","79.90","265.8","332","3.103","dia nonmetal","[Ar] 3d10 4s2 4p5","324.5","2.96","1139, 2103","A. Balard","p","-1, 1, 3, 5, 7","115","114","196.0 (Br-)","114","diamagnetic","molecular\n orthorhombic","7726-95-6","N/A","2.4","32 (Stbl: 2)\nNAT (>=1%):\n 79Br - 50.6%\n 81Br - 49.3%\nMAX LIFE:\n 77Br (57.0h, B+)","21.0","0.122","226 [Br2(l)]"),
E(17,3,36,4,18,"Kr","Krypton","83.79","115.7","119.9","3.7e-3","noble gas","[Ar] 3d10 4s2 4p6","-96.0","3.00","1350, 2350","W. Ramsay","p","N/A","N/A","117","N/A","N/A","diamagnetic","face-centred\n cubic","7439-90-9","N/A","1e-4","33 (Stbl: 5)\nNAT (>=1%):\n 84Kr - 56.9%\n 86Kr - 17.2%\n 82Kr - 11.5%\n 83Kr - 11.4%\n 80Kr - 2.2%\nMAX LIFE:\n 81Kr (229ky)","16.78","0.0094","248 [Kr(g)]"),
E(0,4,37,5,1,"Rb","Rubidium","85.46","312.4","961","1.532","alkali metal","[Kr] 5s1","46.8","0.82","403, 2633","R. Bunsen","s","1","235","210","152.0 (Rb+)","248","paramagnetic","body-centred\n cubic","7440-17-7","0.3","90","33 (Stbl: 1)\nNAT (>=1%):\n 85Rb - 72.1%\n 87Rb - 27.8%\nMAX LIFE:\n 87Rb (49.7Gy, B-)","319.8","58.2","363 [Rb(s)]"),
E(1,4,38,5,2,"Sr","Strontium","87.62","1050","1650","2.64","alkaline earth","[Kr] 5s2","5.0","0.95","549, 1064","W. Cruickshank","s","2","200","185","118.0 (Sr2+)","215","paramagnetic","face-centred\n cubic","7440-24-6","1.5","370","35 (Stbl: 4)\nNAT (>=1%):\n 88Sr - 82.5%\n 86Sr - 9.8%\n 87Sr - 7.0%\nMAX LIFE:\n 90Sr (28.9y, B-)","197.2","35.4","301 [Sr(s)]"),
E(2,4,39,5,3,"Y","Yttrium","88.90","1799","3203","4.472","transition metal","[Kr] 4d1 5s2","29.6","1.22","600, 1180","J. Gadolin","d","3","180","163","90.0 (Y3+)","180","paramagnetic","hexagonal\n close-packed","7440-65-5","N/A","33","33 (Stbl: 1)\nNAT (>=1%):\n 89Y - 100%\nMAX LIFE:\n 88Y (106d, B+)","162.0","17.2","298 [Y(s)]"),
E(3,4,40,5,4,"Zr","Zirconium","91.22","2128","4650","6.52","transition metal","[Kr] 4d2 5s2","41.8","1.33","640, 1270","M. Klaproth","d","4","155","154","72.0 (Zr4+)","160","paramagnetic","hexagonal\n close-packed","7440-67-7","5.0","165","36 (Stbl: 4)\nNAT (>=1%):\n 90Zr - 51.4%\n 94Zr - 17.3%\n 92Zr - 17.1%\n 91Zr - 11.2%\n 96Zr - 2.8%\nMAX LIFE:\n 96Zr (23Ey, B)","112.0","22.6","278 [Zr(s)]"),
E(4,4,41,5,5,"Nb","Niobium","92.90","2750","5017","8.57","transition metal","[Kr] 4d4 5s1","88.5","1.60","652, 1380","C. Hatchett","d","5","145","147","64.0 (Nb5+)","146","paramagnetic","body-centred\n cubic","7440-03-1","6.0","20","35 (Stbl: 1)\nNAT (>=1%):\n 93Nb - 100%\nMAX LIFE:\n 92Nb (34.7My, B+)","98.0","53.7","265 [Nb(s)]"),
E(5,4,42,5,6,"Mo","Molybdenum","95.95","2896","4912","10.28","transition metal","[Kr] 4d5 5s1","72.1","2.16","684, 1560","C. Scheele","d","4, 6","145","138","59.0 (Mo6+)","139","paramagnetic","body-centred\n cubic","7439-98-7","5.5","1.2","38 (Stbl: 6)\nNAT (>=1%):\n 98Mo - 24.3%\n 96Mo - 16.7%\n 95Mo - 15.9%\n 92Mo - 14.6%\n 100Mo - 9.7%\n 97Mo - 9.6%\n 94Mo - 9.2%\nMAX LIFE:\n 100Mo (7.0Ey, B)","87.0","138.0","251 [Mo(s)]"),
E(6,4,43,5,7,"Tc","Technetium","[98]","2430","4538","11.0","transition metal","[Kr] 4d5 5s2","53.0","1.90","702, 1470","E. Segre","d","1...7","135","128","56.0 (Tc7+)","136","paramagnetic","hexagonal\n close-packed","7440-26-8","N/A","3e-15","40 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 98Tc (4.2My, B-)","79.0","50.6","226 [Tc(s)]"),
E(7,4,44,5,8,"Ru","Ruthenium","101.07","2607","4423","12.45","transition metal","[Kr] 4d7 5s1","100.9","2.20","710, 1620","K. Claus","d","-2...8","130","125","68.0 (Ru3+)","134","paramagnetic","hexagonal\n close-packed","7440-18-8","6.5","0.001","41 (Stbl: 7)\nNAT (>=1%):\n 102Ru - 31.5%\n 104Ru - 18.6%\n 101Ru - 17.1%\n 99Ru - 12.8%\n 100Ru - 12.6%\n 96Ru - 5.5%\n 98Ru - 1.8%\nMAX LIFE:\n 106Ru (371d, B)","72.0","117.0","238 [Ru(s)]"),
E(8,4,45,5,9,"Rh","Rhodium","102.90","2237","3968","12.41","transition metal","[Kr] 4d8 5s1","110.2","2.28","719, 1740","W. Wollaston","d","-1,1,2,3,4,5,6","135","125","66.5 (Rh3+)","134","paramagnetic","face-centred\n cubic","7440-16-6","6.0","0.001","38 (Stbl: 1)\nNAT (>=1%):\n 103Rh - 100%\nMAX LIFE:\n 101Rh (4y, EC)","66.0","150.0","243 [Rh(s)]"),
E(9,4,46,5,10,"Pd","Palladium","106.42","1828","3236","12.023","transition metal","[Kr] 4d10","54.2","2.20","804, 1870","W. Wollaston","d","2, 4","140","120","86.0 (Pd2+)","137","paramagnetic","face-centred\n cubic","7440-05-3","4.75","0.015","43 (Stbl: 6)\nNAT (>=1%):\n 106Pd - 27.3%\n 108Pd - 26.5%\n 105Pd - 22.3%\n 110Pd - 11.7%\n 104Pd - 11.1%\n 102Pd - 1.0%\nMAX LIFE:\n 107Pd (6.5My, B)","26.14","71.8","246 [Pd(s)]"),
E(10,4,47,5,11,"Ag","Silver","107.86","1234","2435","10.49","transition metal","[Kr] 4d10 5s1","125.8","1.93","731, 2070","before 5000 BC","d","1","160","128","115.0 (Ag+)","144","diamagnetic","face-centred\n cubic","7440-22-4","2.5","0.075","42 (Stbl: 2)\nNAT (>=1%):\n 107Ag - 51.8%\n 109Ag - 48.1%\nMAX LIFE:\n 108Agm (439y, B+)","55.0","429.0","235 [Ag(s)]"),
E(11,4,48,5,12,"Cd","Cadmium","112.41","594.2","1040","8.65","transition metal","[Kr] 4d10 5s2","-68.0","1.69","867, 1631","K. Hermann","d","2","155","136","95.0 (Cd2+)","151","diamagnetic","hexagonal\n close-packed","7440-43-9","2.0","0.15","45 (Stbl: 6)\nNAT (>=1%):\n 114Cd - 28.7%\n 112Cd - 24.1%\n 111Cd - 12.8%\n 110Cd - 12.5%\n 113Cd - 12.2%\n 116Cd - 7.5%\n 106Cd - 1.2%\nMAX LIFE:\n 116Cd (26.9Ey, B)","46.0","96.6","232 [Cd(s)]"),
E(12,4,49,5,13,"In","Indium","114.81","429.7","2345","7.31","post-transition","[Kr] 4d10 5s2 5p1","37.0","1.78","558, 1820","F. Reich","p","3","155","142","80.0 (In3+)","167","diamagnetic","body-centred\n tetragonal","7440-74-6","1.2","0.25","43 (Stbl: 1)\nNAT (>=1%):\n 115In - 95.7%\n 113In - 4.3%\nMAX LIFE:\n 115In (441Ty, B-)","65.0","81.8","233 [In(s)]"),
E(13,4,50,5,14,"Sn","Tin","118.71","505.0","2875","7.265","post-transition","[Kr] 4d10 5s2 5p2","107.2","1.96","708, 1411","before 3500 BC","p","-4, 2, 4","145","140","69.0 (Sn4+)","158","diamagnetic","body-centred\n tetrag(beta)","7440-31-5","1.5","2.3","50 (Stbl: 10)\nNAT (>=1%):\n 120Sn - 32.6%\n 118Sn - 24.2%\n 116Sn - 14.5%\n 119Sn - 8.6%\n 117Sn - 7.7%\n 124Sn - 5.8%\n 122Sn - 4.6%\nMAX LIFE:\n 126Sn (230ky, B-)","53.0","66.8","228 [Sn(s)]"),
E(14,4,51,5,15,"Sb","Antimony","121.76","903.7","1908","6.697","metalloid","[Kr] 4d10 5s2 5p3","101.0","2.05","834, 1594","before 3000 BC","p","-3, 3, 5","145","140","76.0 (Sb3+)","141","diamagnetic","rhombohedral","7440-36-0","3.0","0.2","41 (Stbl: 2)\nNAT (>=1%):\n 121Sb - 57.2%\n 123Sb - 42.7%\nMAX LIFE:\n 125Sb (2.75y, B-)","43.0","24.4","207 [Sb(s)]"),
E(15,4,52,5,16,"Te","Tellurium","127.60","722.6","1261","6.24","metalloid","[Kr] 4d10 5s2 5p4","190.1","2.10","869, 1790","Reichenstein","p","-2, 2, 4, 6","140","136","221.0 (Te2-)","137","diamagnetic","trigonal\n helical chains","13494-80-7","2.25","0.001","48 (Stbl: 6)\nNAT (>=1%):\n 130Te - 34.0%\n 128Te - 31.7%\n 126Te - 18.8%\n 125Te - 7.1%\n 124Te - 4.7%\n 122Te - 2.5%\nMAX LIFE:\n 128Te (2.25Yy, B)","38.0","3.0","202 [Te(s)]"),
E(16,4,53,5,17,"I","Iodine","126.90","386.8","457.4","4.933","dia nonmetal","[Kr] 4d10 5s2 5p5","295.1","2.66","1008, 1845","B. Courtois","p","-1, 1, 3, 5, 7","140","133","220.0 (I-)","133","diamagnetic","molecular\n orthorhombic","7553-56-2","1.5","0.45","42 (Stbl: 1)\nNAT (>=1%):\n 127I - 100%\nMAX LIFE:\n 129I (16.1My, B-)","33.0","0.449","145 [I2(s)]"),
E(17,4,54,5,18,"Xe","Xenon","131.29","161.4","165.0","5.8e-3","noble gas","[Kr] 4d10 5s2 5p6","-77.0","2.60","1170, 2046","W. Ramsay","p","N/A","N/A","131","N/A","N/A","diamagnetic","face-centred\n cubic","7440-63-3","N/A","3e-5","46 (Stbl: 7)\nNAT (>=1%):\n 132Xe - 26.9%\n 129Xe - 26.4%\n 131Xe - 21.2%\n 134Xe - 10.4%\n 136Xe - 8.9%\n 130Xe - 4.1%\n 128Xe - 1.9%\nMAX LIFE:\n 124Xe (200Ty)","27.3","0.0057","158 [Xe(g)]"),
E(0,5,55,6,1,"Cs","Cesium","132.90","301.7","944","1.93","alkali metal","[Xe] 6s1","45.5","0.79","375, 2234","R. Bunsen","s","1","260","232","167.0 (Cs+)","265","paramagnetic","body-centred\n cubic","7440-46-2","0.2","3.0","42 (Stbl: 1)\nNAT (>=1%):\n 133Cs - 100%\nMAX LIFE:\n 135Cs (1.33My, B-)","400.9","36.0","242 [Cs(s)]"),
E(1,5,56,6,2,"Ba","Barium","137.32","1000","2118","3.51","alkaline earth","[Xe] 6s2","13.9","0.89","502, 965","C. Scheele","s","2","215","196","135.0 (Ba2+)","222","paramagnetic","body-centred\n cubic","7440-39-3","1.25","425","47 (Stbl: 6)\nNAT (>=1%):\n 138Ba - 71.7%\n 137Ba - 11.2%\n 136Ba - 7.9%\n 135Ba - 6.6%\n 134Ba - 2.4%\nMAX LIFE:\n 130Ba (1.0Zy, 2B)","272.0","18.4","204 [Ba(s)]"),
E(2,8,57,6,3,"La","Lanthanum","138.90","1193","3737","6.162","lanthanide","[Xe] 5d1 6s2","53.0","1.10","538, 1067","C. Mosander","f","3","195","180","103.2 (La3+)","187","paramagnetic","double hex\n close-packed","7439-91-0","2.5","39","45 (Stbl: 1)\nNAT (>=1%):\n 139La - 99.9%\nMAX LIFE:\n 138La (103Gy, B+,B-)","205.0","13.4","195 [La(s)]"),
E(3,8,58,6,3,"Ce","Cerium","140.11","1068","3716","6.77","lanthanide","[Xe] 4f1 5d1 6s2","55.0","1.12","534, 1050","M. Klaproth","f","3, 4","185","163","101.0 (Ce3+)","182","paramagnetic","face-centred\n cubic (gamma)","7440-45-1","2.5","66.5","47 (Stbl: 4)\nNAT (>=1%):\n 140Ce - 88.4%\n 142Ce - 11.1%\nMAX LIFE:\n 136Ce (32Py, B)","216.0","11.3","192 [Ce(s)]"),
E(4,8,59,6,3,"Pr","Praseodymium","140.90","1208","3403","6.77","lanthanide","[Xe] 4f3 6s2","93.0","1.13","527, 1020","C. Welsbach","f","3, 4","185","176","99.0 (Pr3+)","182","paramagnetic","double hex\n close-packed","7440-10-0","2.5","9.2","45 (Stbl: 1)\nNAT (>=1%):\n 141Pr - 100%\nMAX LIFE:\n 143Pr (13.57d, B-)","216.0","12.5","193 [Pr(s)]"),
E(5,8,60,6,3,"Nd","Neodymium","144.24","1297","3347","7.01","lanthanide","[Xe] 4f4 6s2","184.8","1.14","533, 1040","C. Welsbach","f","3","185","174","98.3 (Nd3+)","181","paramagnetic","double hex\n close-packed","7440-00-8","2.5","41.5","48 (Stbl: 5)\nNAT (>=1%):\n 142Nd - 27.2%\n 144Nd - 23.8%\n 146Nd - 17.2%\n 143Nd - 12.2%\n 145Nd - 8.3%\n 148Nd - 5.8%\n 150Nd - 5.6%\nMAX LIFE:\n 144Nd (2.29Py, A)","208.0","16.5","190 [Nd(s)]"),
E(6,8,61,6,3,"Pm","Promethium","[145]","1315","3273","7.26","lanthanide","[Xe] 4f5 6s2","12.4","1.13","540, 1050","C. Wu","f","3","185","173","97.0 (Pm3+)","181","paramagnetic","double hex\n close-packed","7440-12-2","N/A","N/A","43 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 145Pm (17.7y, EC)","200.0","17.9","N/A"),
E(7,8,62,6,3,"Sm","Samarium","150.36","1345","2173","7.52","lanthanide","[Xe] 4f6 6s2","15.6","1.17","544, 1070","L. Boisbaudran","f","2, 3","185","172","95.8 (Sm3+)","180","paramagnetic","rhombohedral\n sm-type","7440-19-9","N/A","7.05","48 (Stbl: 5)\nNAT (>=1%):\n 152Sm - 26.7%\n 154Sm - 22.7%\n 147Sm - 15.0%\n 149Sm - 13.8%\n 148Sm - 11.3%\n 150Sm - 7.4%\n 144Sm - 3.1%\nMAX LIFE:\n 148Sm (6.3Py, A)","192.0","13.3","197 [Sm(s)]"),
E(8,8,63,6,3,"Eu","Europium","151.96","1099","1802","5.244","lanthanide","[Xe] 4f7 6s2","11.2","1.20","547, 1085","E. Demarcay","f","2, 3","185","168","94.7 (Eu3+)","208","paramagnetic","body-centred\n cubic","7440-53-1","N/A","2.0","46 (Stbl: 1)\nNAT (>=1%):\n 153Eu - 52.2%\n 151Eu - 47.8%\nMAX LIFE:\n 151Eu (4.6Ey, A)","184.0","13.9","182 [Eu(s)]"),
E(9,8,64,6,3,"Gd","Gadolinium","157.25","1585","3273","7.90","lanthanide","[Xe] 4f7 5d1 6s2","13.2","1.20","593, 1170","J. Marignac","f","3","180","169","93.5 (Gd3+)","180","ferromagnetic","hexagonal\n close-packed","7440-54-2","5.0","6.2","49 (Stbl: 6)\nNAT (>=1%):\n 158Gd - 24.8%\n 160Gd - 21.9%\n 156Gd - 20.5%\n 157Gd - 15.6%\n 155Gd - 14.8%\n 154Gd - 2.2%\nMAX LIFE:\n 152Gd (108Ty)","158.0","10.6","236 [Gd(s)]"),
E(10,8,65,6,3,"Tb","Terbium","158.92","1629","3396","8.23","lanthanide","[Xe] 4f9 6s2","112.4","1.10","565, 1110","C. Mosander","f","3, 4","175","168","92.3 (Tb3+)","177","paramagnetic","hexagonal\n close-packed","7440-27-9","N/A","1.2","46 (Stbl: 1)\nNAT (>=1%):\n 159Tb - 100%\nMAX LIFE:\n 158Tb (180y, B)","170.0","11.1","182 [Tb(s)]"),
E(11,8,66,6,3,"Dy","Dysprosium","162.50","1680","2840","8.54","lanthanide","[Xe] 4f10 6s2","33.9","1.22","573, 1130","L. Boisbaudran","f","3","175","167","91.2 (Dy3+)","178","paramagnetic","hexagonal\n close-packed","7429-91-6","N/A","5.2","49 (Stbl: 7)\nNAT (>=1%):\n 164Dy - 28.3%\n 162Dy - 25.5%\n 163Dy - 24.9%\n 161Dy - 18.9%\n 160Dy - 2.3%\nMAX LIFE:\n 154Dy (3.0My, A)","163.0","10.7","170 [Dy(s)]"),
E(12,8,67,6,3,"Ho","Holmium","164.93","1734","2873","8.79","lanthanide","[Xe] 4f11 6s2","32.6","1.23","581, 1140","Delafontaine","f","3","175","166","90.1 (Ho3+)","176","paramagnetic","hexagonal\n close-packed","7440-60-0","N/A","1.3","46 (Stbl: 1)\nNAT (>=1%):\n 165Ho - 100%\nMAX LIFE:\n 163Ho (4.57ky, EC)","155.0","16.2","165 [Ho(s)]"),
E(13,8,68,6,3,"Er","Erbium","167.25","1802","3141","9.066","lanthanide","[Xe] 4f12 6s2","30.1","1.24","589, 1150","C. Mosander","f","3","175","165","89.0 (Er3+)","176","paramagnetic","hexagonal\n close-packed","7440-52-0","N/A","3.5","48 (Stbl: 6)\nNAT (>=1%):\n 166Er - 33.5%\n 168Er - 27.0%\n 167Er - 22.9%\n 170Er - 14.9%\n 164Er - 1.6%\nMAX LIFE:\n 169Er (9.39d, B-)","150.0","14.5","168 [Er(s)]"),
E(14,8,69,6,3,"Tm","Thulium","168.93","1818","2223","9.32","lanthanide","[Xe] 4f13 6s2","99.0","1.25","596, 1160","P. Cleve","f","3","175","164","88.0 (Tm3+)","176","paramagnetic","hexagonal\n close-packed","7440-30-4","N/A","0.52","45 (Stbl: 1)\nNAT (>=1%):\n 169Tm - 100%\nMAX LIFE:\n 171Tm (1.92y, B-)","144.0","16.9","160 [Tm(s)]"),
E(15,8,70,6,3,"Yb","Ytterbium","173.04","1097","1469","6.90","lanthanide","[Xe] 4f14 6s2","-1.9","1.10","603, 1174","J. Marignac","f","2, 3","175","170","86.8 (Yb3+)","194","paramagnetic","face-centred\n cubic","7440-64-4","N/A","3.2","48 (Stbl: 7)\nNAT (>=1%):\n 174Yb - 32.0%\n 172Yb - 21.7%\n 173Yb - 16.1%\n 171Yb - 14.1%\n 176Yb - 13.0%\n 170Yb - 3.0%\nMAX LIFE:\n 169Yb (32.0d, EC)","139.0","38.5","155 [Yb(s)]"),
E(16,8,71,6,3,"Lu","Lutetium","174.96","1925","3675","9.841","lanthanide","[Xe] 4f14 5d1 6s2","33.4","1.27","523, 1340","G. Urbain","d","3","175","162","86.1 (Lu3+)","173","paramagnetic","hexagonal\n close-packed","7439-94-3","N/A","0.8","46 (Stbl: 1)\nNAT (>=1%):\n 175Lu - 97.4%\n 176Lu - 2.6%\nMAX LIFE:\n 176Lu (37.01Gy)","137.0","16.4","154 [Lu(s)]"),
E(3,5,72,6,4,"Hf","Hafnium","178.49","2506","4876","13.31","transition metal","[Xe] 4f14 5d2 6s2","17.1","1.30","658, 1440","D. Coster","d","4","155","152","71.0 (Hf4+)","159","paramagnetic","hexagonal\n close-packed","7440-58-6","5.5","3.0","47 (Stbl: 5)\nNAT (>=1%):\n 180Hf - 35.1%\n 178Hf - 27.3%\n 177Hf - 18.6%\n 179Hf - 13.6%\n 176Hf - 5.3%\nMAX LIFE:\n 174Hf (2.0Py, A)","103.0","23.0","144 [Hf(s)]"),
E(4,5,73,6,5,"Ta","Tantalum","180.94","3290","5731","16.69","transition metal","[Xe] 4f14 5d3 6s2","31.0","1.50","761, 1500","A. Ekeberg","d","5","145","146","64.0 (Ta5+)","146","paramagnetic","body-centred\n cubic","7440-25-7","6.5","2.0","45 (Stbl: 1)\nNAT (>=1%):\n 181Ta - 99.9%\nMAX LIFE:\n 180Ta (45Py, B)","74.0","57.5","140 [Ta(s)]"),
E(5,5,74,6,6,"W","Tungsten","183.84","3695","6203","19.25","transition metal","[Xe] 4f14 5d4 6s2","78.7","2.36","770, 1700","C. Scheele","d","6","135","137","60.0 (W6+)","139","paramagnetic","body-centred\n cubic","7440-33-7","7.5","1.25","47 (Stbl: 4)\nNAT (>=1%):\n 184W - 30.6%\n 186W - 28.4%\n 182W - 26.5%\n 183W - 14.3%\nMAX LIFE:\n 180W (1.59Ey, A)","68.0","173.0","132 [W(s)]"),
E(6,5,75,6,7,"Re","Rhenium","186.20","3459","5869","21.02","transition metal","[Xe] 4f14 5d5 6s2","5.8","1.90","760, 1260","M. Ogawa","d","-1...7","135","131","53.0 (Re7+)","137","paramagnetic","hexagonal\n close-packed","7440-15-5","7.0","7e-4","46 (Stbl: 1)\nNAT (>=1%):\n 187Re - 62.6%\n 185Re - 37.4%\nMAX LIFE:\n 187Re (41.6Gy, B-)","62.0","48.0","137 [Re(s)]"),
E(7,5,76,6,8,"Os","Osmium","190.23","3306","5285","22.59","transition metal","[Xe] 4f14 5d6 6s2","103.9","2.20","840, 1600","S. Tennant","d","-2...8","130","129","63.0 (Os4+)","135","paramagnetic","hexagonal\n close-packed","7440-04-2","7.0","0.0015","48 (Stbl: 6)\nNAT (>=1%):\n 192Os - 40.8%\n 190Os - 26.3%\n 189Os - 16.2%\n 188Os - 13.2%\n 187Os - 2.0%\n 186Os - 1.6%\nMAX LIFE:\n 186Os (2.0Py, A)","57.0","87.6","130 [Os(s)]"),
E(8,5,77,6,9,"Ir","Iridium","192.21","2719","4403","22.56","transition metal","[Xe] 4f14 5d7 6s2","150.9","2.20","880, 1600","S. Tennant","d","-1,1...6","135","122","68.0 (Ir3+)","136","paramagnetic","face-centred\n cubic","7439-88-5","6.5","0.001","47 (Stbl: 2)\nNAT (>=1%):\n 193Ir - 62.7%\n 191Ir - 37.3%\nMAX LIFE:\n 192Irn (241.0y, IT)","54.0","147.0","131 [Ir(s)]"),
E(9,5,78,6,10,"Pt","Platinum","195.08","2041","4098","21.45","transition metal","[Xe] 4f14 5d9 6s1","205.0","2.28","870, 1791","A. Ulloa","d","2, 4","135","123","80.0 (Pt2+)","139","paramagnetic","face-centred\n cubic","7440-06-4","3.5","0.005","49 (Stbl: 5)\nNAT (>=1%):\n 195Pt - 33.8%\n 194Pt - 32.9%\n 196Pt - 25.2%\n 198Pt - 7.4%\nMAX LIFE:\n 190Pt (483.0Gy, A)","48.0","71.6","133 [Pt(s)]"),
E(10,5,79,6,11,"Au","Gold","196.96","1337","3243","19.3","transition metal","[Xe] 4f14 5d10 6s1","222.7","2.54","890, 1980","Middle East","d","3","135","124","85.0 (Au3+)","144","diamagnetic","face-centred\n cubic","7440-57-5","2.5","0.004","39 (Stbl: 1)\nNAT (>=1%):\n 197Au - 100%\nMAX LIFE:\n 195Au (186.0d, EC)","35.0","318.0","129 [Au(s)]"),
E(11,5,80,6,12,"Hg","Mercury","200.59","234.3","629.8","13.534","transition metal","[Xe] 4f14 5d10 6s2","-48.0","2.00","1007, 1810","2000 BCE","d","2","150","133","102.0 (Hg2+)","151","diamagnetic","rhombohedral\n alpha-form","7439-97-6","N/A","0.085","49 (Stbl: 7)\nNAT (>=1%):\n 202Hg - 29.7%\n 200Hg - 23.1%\n 199Hg - 16.9%\n 201Hg - 13.2%\n 198Hg - 10.0%\n 204Hg - 6.8%\nMAX LIFE:\n 194Hg (447.0y, EC)","33.9","8.3","140 [Hg(l)]"),
E(12,5,81,6,13,"Tl","Thallium","204.38","577","1746","11.85","post-transition","[Xe] 4f14 5d10 6s2 6p1","36.4","1.62","589, 1971","W. Crookes","p","1, 3","190","144","150.0 (Tl+)","171","diamagnetic","hexagonal\n close-packed","7440-28-0","1.2","0.85","47 (Stbl: 2)\nNAT (>=1%):\n 205Tl - 70.5%\n 203Tl - 29.5%\nMAX LIFE:\n 204Tl (3.78y, B-)","50.0","46.1","129 [Tl(s)]"),
E(13,5,82,6,14,"Pb","Lead","207.21","600.6","2022","11.34","post-transition","[Xe] 4f14 5d10 6s2 6p2","34.4","1.87","715, 1450","Middle East","p","2, 4","180","144","119.0 (Pb2+)","175","diamagnetic","face-centred\n cubic","7439-92-1","1.5","14","49 (Stbl: 4)\nNAT (>=1%):\n 208Pb - 52.4%\n 206Pb - 24.1%\n 207Pb - 22.1%\n 204Pb - 1.4%\nMAX LIFE:\n 204Pb (140.0Py)","47.0","35.3","129 [Pb(s)]"),
E(14,5,83,6,15,"Bi","Bismuth","208.98","544.7","1837","9.78","post-transition","[Xe] 4f14 5d10 6s2 6p3","90.9","2.02","703, 1610","C. Geoffroy","p","3, 5","160","151","103.0 (Bi3+)","182","diamagnetic","rhombohedral","7440-69-9","2.25","0.0085","46 (Stbl: 0)\nNAT (>=1%):\n 209Bi - 100%\nMAX LIFE:\n 209Bi (20.1Ey, A)","48.0","7.97","122 [Bi(s)]"),
E(15,5,84,6,16,"Po","Polonium","[209]","527","1235","9.196","post-transition","[Xe] 4f14 5d10 6s2 6p4","136.0","2.00","812.1","P. Curie","p","2, 4, 6","190","145","94.0 (Po4+)","168","diamagnetic","simple cubic","7440-08-6","N/A","2e-10","44 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 209Po (124.0y, A)","44.0","20.0","N/A"),
E(16,5,85,6,17,"At","Astatine","[210]","575","610","7.0","metalloid","[Xe] 4f14 5d10 6s2 6p5","233.0","2.20","899.0","D. Corson","p","-1, 1, 3, 5, 7","N/A","147","62.0 (At-)","N/A","N/A","N/A","7440-68-8","N/A","3e-27","41 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 210At (8.1h, B+)","42.0","1.7","N/A"),
E(17,5,86,6,18,"Rn","Radon","[222]","202","211.5","9.7e-3","noble gas","[Xe] 4f14 5d10 6s2 6p6","-68.0","2.20","1037","F. Dorn","p","N/A","N/A","142","N/A","N/A","diamagnetic","face-centred\n cubic","10043-92-2","N/A","4e-13","42 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 222Rn (3.82d, A)","35.0","0.0036","94 [Rn(g)]"),
E(0,6,87,7,1,"Fr","Francium","[223]","300","950","2.48","alkali metal","[Rn] 7s1","46.8","0.79","380.0","M. Perey","s","1","N/A","223","180.0 (Fr+)","N/A","N/A","N/A","7440-73-5","N/A","1e-24","40 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 223Fr (22.0m, B-)","320.0","15.0","N/A"),
E(1,6,88,7,2,"Ra","Radium","[226]","1233","2010","5.5","alkaline earth","[Rn] 7s2","9.6","0.90","509, 979","P. Curie","s","2","215","201","148.0 (Ra2+)","215","N/A","body-centred\n cubic","7440-14-4","N/A","9e-07","41 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 226Ra (1.6ky, A)","246.0","18.6","92 [Ra(s)]"),
E(2,9,89,7,3,"Ac","Actinium","[227]","1500","3500","10.0","actinide","[Rn] 6d1 7s2","33.7","1.10","499, 1170","F. Giesel","f","3","195","186","106.5 (Ac3+)","188","N/A","face-centred\n cubic","7440-34-8","N/A","5.5e-10","39 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 227Ac (21.7y, B-)","203.0","12.0","120 [Ac(s)]"),
E(3,9,90,7,3,"Th","Thorium","[232]","2023","5061","11.7","actinide","[Rn] 6d2 7s2","112.7","1.30","587, 1110","J. Berzelius","f","4","180","175","94.0 (Th4+)","179","paramagnetic","face-centred\n cubic","7440-29-1","3.0","9.6","40 (Stbl: 0)\nNAT (>=1%):\n 232Th - 100%\nMAX LIFE:\n 232Th (14.0Gy, A)","217.0","54.0","113 [Th(s)]"),
E(4,9,91,7,3,"Pa","Protactinium","[231]","1841","4300","15.37","actinide","[Rn] 5f2 6d1 7s2","53.0","1.50","568.0","W. Crookes","f","5","180","169","78.0 (Pa5+)","163","paramagnetic","bc-tetragonal","7440-13-3","N/A","1.4e-06","38 (Stbl: 0)\nNAT (>=1%):\n 231Pa - 100%\nMAX LIFE:\n 231Pa (32.6ky, A)","154.0","47.0","N/A"),
E(5,9,92,7,3,"U","Uranium","[238]","1405","4404","19.1","actinide","[Rn] 5f3 6d1 7s2","50.9","1.38","597, 1420","M. Klaproth","f","3...6","175","170","73.0 (U6+)","156","paramagnetic","orthorhombic","7440-61-1","6.0","2.7","41 (Stbl: 0)\nNAT (>=1%):\n 238U - 99.3%\nMAX LIFE:\n 238U (4.46Gy, A)","129.0","27.6","116 [U(s)]"),
E(6,9,93,7,3,"Np","Neptunium","[237]","912","4447","20.2","actinide","[Rn] 5f4 6d1 7s2","45.8","1.36","604.5","E. McMillan","f","3...7","175","171","71.0 (Np7+)","155","paramagnetic","orthorhombic","7439-99-8","N/A","N/A","39 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 237Np (2.14My, A)","151.0","6.3","N/A"),
E(7,9,94,7,3,"Pu","Plutonium","[244]","912.5","3505","19.816","actinide","[Rn] 5f6 7s2","-48.3","1.28","584.7","G. Seaborg","f","3...7","175","172","86.0 (Pu4+)","159","paramagnetic","monoclinic","7440-07-5","N/A","N/A","40 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 244Pu (81.3My, A)","132.0","6.7","N/A"),
E(8,9,95,7,3,"Am","Americium","[243]","1449","2880","12.0","actinide","[Rn] 5f7 7s2","9.9","1.13","578.0","G. Seaborg","f","3...6","175","166","97.5 (Am3+)","173","paramagnetic","double hex\n close-packed","7440-35-9","N/A","N/A","38 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 243Am (7.35ky, A)","131.0","10.0","N/A"),
E(9,9,96,7,3,"Cm","Curium","[247]","1613","3383","13.51","actinide","[Rn] 5f7 6d1 7s2","27.1","1.28","581.0","G. Seaborg","f","3","N/A","166","97.0 (Cm3+)","174","paramagnetic","double hex\n close-packed","7440-51-9","N/A","N/A","39 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 247Cm (15.6My, A)","144.0","10.0","N/A"),
E(10,9,97,7,3,"Bk","Berkelium","[247]","1259","2900","14.78","actinide","[Rn] 5f9 7s2","-165.2","1.30","601.0","LBNL","f","3, 4","N/A","168","96.0 (Bk3+)","170","paramagnetic","double hex\n close-packed","7440-40-6","N/A","N/A","37 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 247Bk (1.38ky, A)","125.0","10.0","N/A"),
E(11,9,98,7,3,"Cf","Californium","[251]","1173","1743","15.1","actinide","[Rn] 5f10 7s2","-97.3","1.30","608.0","LBNL","f","2, 3, 4","N/A","168","95.0 (Cf3+)","186","paramagnetic","double hex\n close-packed","7440-71-3","N/A","N/A","38 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 251Cf (898.0y, A)","122.0","10.0","N/A"),
E(12,9,99,7,3,"Es","Einsteinium","[252]","1133","1269","8.84","actinide","[Rn] 5f11 7s2","-28.6","1.30","619.0","LBNL","f","3","N/A","165","83.5 (Es3+)","186","N/A","face-centred\n cubic","7429-92-7","N/A","N/A","36 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 252Es (471.7d, A)","118.0","N/A","N/A"),
E(13,9,100,7,3,"Fm","Fermium","[257]","1800","N/A","N/A","actinide","[Rn] 5f12 7s2","33.9","1.30","627.0","LBNL","f","3","N/A","167","N/A","N/A","N/A","N/A","7440-72-4","N/A","N/A","35 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 257Fm (100.5d, A)","113.0","N/A","N/A"),
E(14,9,101,7,3,"Md","Mendelevium","[258]","1100","N/A","N/A","actinide","[Rn] 5f13 7s2","93.9","1.30","635.0","LBNL","f","2, 3","N/A","173","N/A","N/A","N/A","N/A","7440-03-1","N/A","N/A","34 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 258Md (51.59d, A)","109.0","N/A","N/A"),
E(15,9,102,7,3,"No","Nobelium","[259]","1100","N/A","N/A","actinide","[Rn] 5f14 7s2","-223.2","1.30","642.0","JINR","f","2, 3","N/A","176","N/A","N/A","N/A","N/A","7440-05-3","N/A","N/A","33 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 259No (58.0m, A)","110.0","N/A","N/A"),
E(16,9,103,7,3,"Lr","Lawrencium","[266]","1900","N/A","N/A","actinide","[Rn] 5f14 7s2 7p1","-30.0","1.30","470.0","LBNL","d","3","N/A","161","N/A","N/A","N/A","N/A","22537-19-3","N/A","N/A","32 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 262Lr (4.0h, B+)","320.0","N/A","N/A"),
E(3,6,104,7,4,"Rf","Rutherfordium","[267]","2400","5800","N/A","transition metal","[Rn] 5f14 6d2 7s2","N/A","N/A","580.0","JINR","d","4","N/A","157","N/A","N/A","N/A","N/A","53850-35-4","N/A","N/A","30 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 267Rf (2.5h, SF)","112.0","N/A","N/A"),
E(4,6,105,7,5,"Db","Dubnium","[268]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d3 7s2","N/A","N/A","N/A","JINR","d","5","N/A","149","N/A","N/A","N/A","N/A","53850-36-5","N/A","N/A","29 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 268Db (29.0h, SF)","42.0","N/A","N/A"),
E(5,6,106,7,6,"Sg","Seaborgium","[269]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d4 7s2","N/A","N/A","N/A","LBNL","d","6","N/A","143","N/A","N/A","N/A","N/A","54038-81-2","N/A","N/A","28 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 269Sg (5.0m, SF)","40.0","N/A","N/A"),
E(6,6,107,7,7,"Bh","Bohrium","[270]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d5 7s2","N/A","N/A","N/A","GSI","d","7","N/A","141","N/A","N/A","N/A","N/A","54037-14-8","N/A","N/A","27 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 270Bh (3.8m, A)","38.0","N/A","N/A"),
E(7,6,108,7,8,"Hs","Hassium","[269]","126","N/A","N/A","transition metal","*[Rn] 5f14 6d6 7s2","N/A","N/A","N/A","GSI","d","8","N/A","134","N/A","N/A","N/A","N/A","54037-57-9","N/A","N/A","26 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 277Hsm (130.0s, SF)","36.0","N/A","N/A"),
E(8,6,109,7,9,"Mt","Meitnerium","[278]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d7 7s2","N/A","N/A","N/A","GSI","d","N/A","N/A","129","N/A","N/A","N/A","N/A","54038-01-6","N/A","N/A","25 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 278Mt (6.0s, A)","34.0","N/A","N/A"),
E(9,6,110,7,10,"Ds","Darmstadtium","[281]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d9 7s1","N/A","N/A","N/A","GSI","d","N/A","N/A","128","N/A","N/A","N/A","N/A","54083-77-1","N/A","N/A","24 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 281Ds (14.0s, SF)","32.0","N/A","N/A"),
E(10,6,111,7,11,"Rg","Roentgenium","[282]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d10 7s1","151.0","N/A","N/A","GSI","d","N/A","N/A","121","N/A","N/A","N/A","N/A","54084-26-3","N/A","N/A","23 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 282Rg (130.0s, A)","32.0","N/A","N/A"),
E(11,6,112,7,12,"Cn","Copernicium","[285]","N/A","3570","N/A","transition metal","*[Rn] 5f14 6d10 7s2","N/A","N/A","N/A","GSI","d","N/A","N/A","122","N/A","N/A","N/A","N/A","54084-26-3","N/A","N/A","22 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 285Cn (30.0s, A)","28.0","N/A","N/A"),
E(12,6,113,7,13,"Nh","Nihonium","[286]","700","1430","N/A","transition metal","*[Rn] 5f14 6d10 7s2 7p1","66.6","N/A","N/A","RIKEN","p","N/A","N/A","136","N/A","N/A","N/A","N/A","N/A","N/A","N/A","21 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 287Nh (20.0s, A)","29.0","N/A","N/A"),
E(13,6,114,7,14,"Fl","Flerovium","[289]","340","420","14.0","post-transition","*[Rn] 5f14 6d10 7s2 7p2","N/A","N/A","N/A","JINR","p","N/A","N/A","143","N/A","N/A","N/A","N/A","N/A","N/A","N/A","20 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 289Fl (2.1s, A)","31.0","N/A","N/A"),
E(14,6,115,7,15,"Mc","Moscovium","[290]","670","1400","N/A","post-transition","*[Rn] 5f14 6d10 7s2 7p3","35.3","N/A","N/A","JINR","p","N/A","N/A","162","N/A","N/A","N/A","N/A","N/A","N/A","N/A","19 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 291Mc (1.0s, A)","71.0","N/A","N/A"),
E(15,6,116,7,16,"Lv","Livermorium","[293]","709","1085","N/A","post-transition","*[Rn] 5f14 6d10 7s2 7p4","74.9","N/A","N/A","JINR","p","N/A","N/A","175","N/A","N/A","N/A","N/A","N/A","N/A","N/A","18 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 293Lv (70.0ms, A)","76.0","N/A","N/A"),
E(16,6,117,7,17,"Ts","Tennessine","[294]","723","883","N/A","metalloid","*[Rn] 5f14 6d10 7s2 7p5","165.9","N/A","N/A","JINR","p","N/A","N/A","165","N/A","N/A","N/A","N/A","N/A","N/A","N/A","17 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 294Ts (70.0ms, A)","58.0","N/A","N/A"),
E(17,6,118,7,18,"Og","Oganesson","[294]","N/A","350","5.0","noble gas","*[Rn] 5f14 6d10 7s2 7p6","5.4","N/A","N/A","JINR","p","N/A","N/A","157","N/A","N/A","N/A","N/A","N/A","N/A","N/A","16 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n 295Og (680.0ms, A)","169.0","N/A","N/A"),
E(0,7,119,8,1,"Uue","Ununennium","[315]","N/A","630","N/A","alkali metal","*[Uuo] 8s1","63.8","N/A","N/A","GSI","s","N/A","N/A","N/A","N/A","N/A","N/A","N/A","N/A","N/A","N/A","0 (Stbl: 0)\nNAT (>=1%):\n N/A\nMAX LIFE:\n N/A","159.0","N/A","N/A")
};

static const Element* get_elm_xy(uint8_t c, uint8_t r) {
    for(uint16_t i=0; i<119; i++) if(DB[i].col == c && DB[i].row == r) return &DB[i];
    return NULL; 
}

static const Element* get_elm_z(uint8_t z) {
    if(z > 0 && z <= 119) return &DB[z-1]; 
    return NULL;
}

typedef struct { uint8_t cur_x; uint8_t cur_y; } ViewModel;
typedef struct { int scroll; char txt[2048]; } DetModel;

// Модель для хранения позиции скролла About меню
typedef struct { int scroll; } AboutModel; 

typedef struct {
    Gui* gui; ViewDispatcher* disp; 
    View* v_splash; View* v_grid;         
    View* v_exit_splash; View* v_details; 
    View* v_about;    
    TextBox* v_refs; TextBox* v_thanks;
    DialogEx* dialog; FuriTimer* t_splash; FuriTimer* t_exit; 
} App;

static void draw_multiline(Canvas* canvas, int x, int y, const char* text) {
    int current_y = y; char buffer[64]; const char* ptr = text;
    while(ptr != NULL && *ptr != '\0') {
        const char* next = strchr(ptr, '\n');
        size_t len = next ? (size_t)(next - ptr) : strlen(ptr);
        if(len < sizeof(buffer)) {
            memcpy(buffer, ptr, len); buffer[len] = '\0';
            if (current_y > -10 && current_y < 70) {
                canvas_draw_str(canvas, x, current_y, buffer);
            }
        }
        current_y += 11;
        if(next) {
            ptr = next + 1;
        } else {
            break;
        }
    }
}

static void start_splash_draw(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, canvas_width(canvas), canvas_height(canvas));

    canvas_set_color(canvas, ColorBlack);

    canvas_draw_xbm(
        canvas,
        8,
        8,
        LAB66_LOGO_WIDTH,
        LAB66_LOGO_HEIGHT,
        lab66_logo_bits);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 66, 19, "PERIODIC");
    canvas_draw_str(canvas, 66, 35, "TABLE OF");
    canvas_draw_str(canvas, 66, 51, "ELEMENTS");
}

static void s_t_cb(void* ctx) { 
    view_dispatcher_send_custom_event(((App*)ctx)->disp, EVENT_START_APP); 
}

static void s_en_cb(void* ctx) { 
    furi_timer_start(((App*)ctx)->t_splash, furi_ms_to_ticks(2000)); 
}

static void exit_splash_draw(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, canvas_width(canvas), canvas_height(canvas));

    canvas_set_color(canvas, ColorBlack);
    
    canvas_draw_frame(canvas, 0, 0, canvas_width(canvas), canvas_height(canvas));

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        64,
        20,
        AlignCenter,
        AlignCenter,
        "Flipper Elements v1.8");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        64,
        34,
        AlignCenter,
        AlignCenter,
        "LAB-66 (C) Siarhei Besarab");

    canvas_draw_str_aligned(
        canvas,
        64,
        48,
        AlignCenter,
        AlignCenter,
        "September 2026");
}

static void e_t_cb(void* ctx) { 
    view_dispatcher_send_custom_event(((App*)ctx)->disp, EVENT_FINAL_CLOSE); 
}

static void e_en_cb(void* ctx) { 
    furi_timer_start(((App*)ctx)->t_exit, furi_ms_to_ticks(1500)); 
}

static void diag_res_cb(DialogExResult r, void* ctx) {
    App* app = (App*)ctx;
    if (r == DialogExResultLeft) {
        view_dispatcher_switch_to_view(app->disp, VIEW_GRID);
    } else if (r == DialogExResultRight) {
        view_dispatcher_switch_to_view(app->disp, VIEW_EXIT_SPLASH);
    }
}

static void grid_draw(Canvas* canvas, void* _m) {
    ViewModel* model = (ViewModel*)_m;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    int s_col = (model->cur_x < 9) ? 0 : 9;
    int e_col = s_col + 8; 
    int s_sz = 5, offset = 6; 

    for(int y = 0; y <= 9; y++) {
        for(int x = s_col; x <= e_col; x++) {
            if (!get_elm_xy(x, y)) continue; 
            int px = (x - s_col) * offset; int py = y * offset + 2; 
            if (y == model->cur_y && x == model->cur_x) {
                canvas_draw_box(canvas, px, py, s_sz, s_sz);
            } else {
                canvas_draw_frame(canvas, px, py, s_sz, s_sz);
            }
        }
    }
    
    canvas_draw_line(canvas, 62, 0, 62, 63); 
    const Element* elm = get_elm_xy(model->cur_x, model->cur_y);
    if (elm) {
        char d[32]; snprintf(d, sizeof(d), "%d", elm->z);
        canvas_set_font(canvas, FontSecondary); canvas_draw_str(canvas, 67, 10, d);
        canvas_set_font(canvas, FontPrimary); canvas_draw_str_aligned(canvas, 95, 20, AlignCenter, AlignCenter, elm->symbol);
        canvas_set_font(canvas, FontSecondary); canvas_draw_str_aligned(canvas, 95, 33, AlignCenter, AlignCenter, elm->name);
        snprintf(d, sizeof(d), "P: %d | G: %d", elm->period, elm->group); canvas_draw_str_aligned(canvas, 95, 45, AlignCenter, AlignCenter, d);
        snprintf(d, sizeof(d), "M: %s", elm->mass); canvas_draw_str_aligned(canvas, 95, 57, AlignCenter, AlignCenter, d);
    }
}

static bool grid_input(InputEvent* ev, void* _ctx) {
    App* app = (App*)_ctx; 
    if (ev->key == InputKeyOk) {
        if(ev->type == InputTypeShort) {
            uint8_t tx = 0, ty = 0;
            with_view_model(app->v_grid, ViewModel* m, { tx = m->cur_x; ty = m->cur_y; }, false);
            if (get_elm_xy(tx, ty)) {
                view_dispatcher_send_custom_event(app->disp, EVENT_SHOW_DETAILS);
            }
            return true;
        } else if (ev->type == InputTypeLong) {
            view_dispatcher_send_custom_event(app->disp, EVENT_SHOW_ABOUT); 
            return true;
        }
    }

    if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
        with_view_model(app->v_grid, ViewModel* m, {
            if (ev->key == InputKeyRight) {
                const Element* cr = get_elm_xy(m->cur_x, m->cur_y);
                if (cr) { 
                    const Element* nx = get_elm_z(cr->z + 1); 
                    if(nx) { m->cur_x = nx->col; m->cur_y = nx->row; } 
                }
            } else if (ev->key == InputKeyLeft) {
                const Element* cr = get_elm_xy(m->cur_x, m->cur_y);
                if (cr) { 
                    const Element* pv = get_elm_z(cr->z - 1); 
                    if(pv) { m->cur_x = pv->col; m->cur_y = pv->row; } 
                }
            } else if (ev->key == InputKeyUp) {
                int ty = m->cur_y; 
                while(ty > 0) { 
                    ty--; 
                    if(get_elm_xy(m->cur_x, ty)) { m->cur_y = ty; break; } 
                }
            } else if (ev->key == InputKeyDown) {
                int ty = m->cur_y; 
                while(ty < 9) { 
                    ty++; 
                    if(get_elm_xy(m->cur_x, ty)) { m->cur_y = ty; break; } 
                }
            }
        }, true); 
        return true; 
    } 
    return false;
}

#define U(val, unit) (strcmp((val), "N/A") == 0 ? "" : (unit))

static void sync_det_txt(View* v_det, uint8_t nx, uint8_t ny) {
    const Element* e = get_elm_xy(nx, ny);
    if(e) {
        with_view_model(v_det, DetModel* m, {
            m->scroll = 0; 
            
            char heat_buf[64] = "N/A";
            if (e->sheat && strcmp(e->sheat, "N/A") != 0) {
                char val[16] = {0};
                const char* space = strchr(e->sheat, ' '); 
                
                if (space && (size_t)(space - e->sheat) < sizeof(val)) {
                    size_t len = space - e->sheat;
                    memcpy(val, e->sheat, len);
                    val[len] = '\0';
                    snprintf(heat_buf, sizeof(heat_buf), "%s J/(Kg.K)%s", val, space);
                } else {
                    snprintf(heat_buf, sizeof(heat_buf), "%s J/(Kg.K)", e->sheat);
                }
            }

            snprintf(m->txt, sizeof(m->txt), 
                 "[ %s (%s) ] Z:%d\n-----------------\nDISCOVERER:\n %s\nCAS: %s\n"
                 "=== ATOMIC INFO ===\nCAT: %s\nBLK: %s | OX: %s\nGRP: %d | PER: %d\nCRYST:\n %s\n"
                 "=== ISOTOPES ===\nTOTAL: %s\n=== SIZES (pm) ===\nATM: %s | COV: %s\nION: %s\nMET: %s\n"
                 "=== PHYSICAL ===\nMASS: %s%s\nDENS: %s%s\nMELT: %s%s\nBOIL: %s%s\nTHRM CONDCT:\n %s%s\nHEAT CAP:\n %s\n"
                 "HARDNESS: %s%s\nABUND: %s%s\nMAGN: %s\n"
                 "=== QUANTUM ===\nAFFIN: %s%s\nEL.NG: %s\nION_E: %s%s\nPOLAR: %s%s\nORBIT:\n %s", 
                 e->name, e->symbol, e->z, e->disc, e->cas, e->cat, e->blk, e->ox, e->group, e->period, 
                 e->cryst, e->iso, e->r_a, e->r_c, e->r_i, e->r_m, 
                 
                 e->mass, U(e->mass, " u"),
                 e->dens, U(e->dens, " g/cm3"), 
                 e->melt, U(e->melt, " K"), 
                 e->boil, U(e->boil, " K"), 
                 e->tcond, U(e->tcond, " W/(m.K)"), 
                 heat_buf, 
                 e->mohs, U(e->mohs, " Mohs"), 
                 e->abund, U(e->abund, " ppm"), 
                 e->mag, 
                 
                 e->aff, U(e->aff, " kJ/mol"),
                 e->pauling, 
                 e->ie, U(e->ie, " kJ/mol"), 
                 e->polar, U(e->polar, " a.u."), 
                 e->e_conf);
        }, true); 
    }
}
#undef U

static void det_draw(Canvas* canvas, void* _m) {
    DetModel* model = (DetModel*)_m;
    canvas_clear(canvas); canvas_set_color(canvas, ColorBlack); canvas_set_font(canvas, FontSecondary);
    draw_multiline(canvas, 2, 10 - model->scroll, model->txt);
    
    int sb_y = (model->scroll * 56) / 590; 
    if (sb_y > 56) {
        sb_y = 56;
    } 
    if (sb_y < 0) {
        sb_y = 0; 
    }
    
    canvas_draw_box(canvas, 126, sb_y, 2, 8); 
}

static bool det_input(InputEvent* ev, void* _ctx) {
    App* a = (App*)_ctx;
    if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
        
        if(ev->key == InputKeyUp) { 
            with_view_model(a->v_details, DetModel* m, { 
                if(m->scroll > 0) m->scroll -= 11; 
            }, true); 
            return true; 
        }
        
        if(ev->key == InputKeyDown) { 
            with_view_model(a->v_details, DetModel* m, { 
                if(m->scroll < 590) m->scroll += 11; 
            }, true); 
            return true; 
        }
        
        if(ev->key == InputKeyRight || ev->key == InputKeyLeft) { 
            uint8_t nx = 0, ny = 0; bool fd = false;
            
            with_view_model(a->v_grid, ViewModel* gm, { 
                nx = gm->cur_x; ny = gm->cur_y; 
            }, false);
            
            const Element* c_e = get_elm_xy(nx, ny);
            if(c_e) {
                const Element* tar = NULL;
                if(ev->key == InputKeyRight) {
                    tar = get_elm_z(c_e->z + 1);
                } else {
                    tar = get_elm_z(c_e->z - 1);
                }
                
                if(tar) { 
                    nx = tar->col; 
                    ny = tar->row; 
                    fd = true; 
                    with_view_model(a->v_grid, ViewModel* gm2, { 
                        gm2->cur_x = nx; 
                        gm2->cur_y = ny; 
                    }, false); 
                }
            }
            if(fd) {
                sync_det_txt(a->v_details, nx, ny);
            }
            return true;
        }
    }
    return false;
}

// ========================== НОВЫЙ КАСТОМНЫЙ ABOUT VIEW С БАМПЕРАМИ ==========================
static void about_draw(Canvas* canvas, void* _m) {
    AboutModel* model = (AboutModel*)_m;
    canvas_clear(canvas); 
    canvas_set_color(canvas, ColorBlack);
    
    // Центральный скроллируемый текст 
    const char* txt_main = 
        "Flipper Elements v1.8\n\n"
        "Periodic Table of Elements\n"
        "for Flipper Zero.\n\n"
        "Author: Siarhei Besarab\n"
        "aka steanlab.\n\n"
        "Contacts:\n"
        "LAB-66: t.me/lab66\n"
        "Linkedin: @steanlab\n"
        "Mastodon: @lab66\n"
        "---------------\n"
        "Support Development:\n"
        "patreon.com/steanlab\n"
        "paypal.me/steanlab\n"
        "revolut.me/steanlab\n"
        "github.com/sponsors/\nsteanlab\n"
        "donorbox.org/donations-\n"
        "for-lab-66\n\n\n "; 

    canvas_set_font(canvas, FontSecondary);
    draw_multiline(canvas, 2, 10 - model->scroll, txt_main);

    // БЕЛЫЙ ПЕРЕКРЫВАЮЩИЙ ФУТЕР-БЛОК ДЛЯ МЕНЮ
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 52, 128, 12);
    
    // ОТРИСОВКА ЧЕРНОЙ КНОПОЧНОЙ ПАНЕЛИ И ГРАНИЦ
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 51, 128, 51); // верхняя разделительная линия
    
    // Рисуем кнопки управления (четко по ТЗ пользователя)
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "[ < Refs ]");
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "[ Thanks > ]");

    // Индикатор скролла 
    int sb_y = (model->scroll * 40) / 190; // ~ 190px - макс. размер скролла основного текста
    if (sb_y > 42) sb_y = 42; 
    if (sb_y < 0) sb_y = 0; 
    canvas_draw_box(canvas, 126, sb_y, 2, 8); 
}

static bool about_input(InputEvent* ev, void* _ctx) {
    App* a = (App*)_ctx;
    
    if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
        
        // Листаем вверх-вниз центральный текст
        if(ev->key == InputKeyUp) { 
            with_view_model(a->v_about, AboutModel* m, { 
                if(m->scroll > 0) m->scroll -= 11; 
            }, true); 
            return true; 
        }
        if(ev->key == InputKeyDown) { 
            with_view_model(a->v_about, AboutModel* m, { 
                if(m->scroll < 200) m->scroll += 11; 
            }, true); 
            return true; 
        }

        // Переходим к окошкам Sources (Refs) или Thanks (Right)
        if(ev->key == InputKeyLeft) {
            view_dispatcher_send_custom_event(a->disp, EVENT_SHOW_REFS); 
            return true; 
        }
        if(ev->key == InputKeyRight) {
            view_dispatcher_send_custom_event(a->disp, EVENT_SHOW_THANKS); 
            return true; 
        }
    }
    // Кнопка BACK проходит мимо, так как она привязана в view_set_previous_callback
    return false; 
}


// РОУТЕРЫ 
static uint32_t grid_b(void* ctx) { UNUSED(ctx); return VIEW_EXIT_PROMPT; }
static uint32_t to_grid(void* ctx) { UNUSED(ctx); return VIEW_GRID; }
static uint32_t to_about(void* ctx) { UNUSED(ctx); return VIEW_ABOUT; }


// ИВЕНТ-ЛУП
static bool main_ev(void* ctx, uint32_t ev) {
    App* a = (App*)ctx;
    if (ev == EVENT_START_APP) { view_dispatcher_switch_to_view(a->disp, VIEW_GRID); return true; }
    if (ev == EVENT_FINAL_CLOSE) { view_dispatcher_stop(a->disp); return true; }
    
    if (ev == EVENT_SHOW_DETAILS) {
        uint8_t x=0, y=0; 
        with_view_model(a->v_grid, ViewModel* m, { x=m->cur_x; y=m->cur_y; }, false);
        sync_det_txt(a->v_details, x, y); 
        view_dispatcher_switch_to_view(a->disp, VIEW_DETAILS); 
        return true;
    }

    if (ev == EVENT_SHOW_ABOUT) {
        // Обязательно сбрасываем скролл, чтобы при возврате экран был на верху
        with_view_model(a->v_about, AboutModel* m, { m->scroll = 0; }, true);
        view_dispatcher_switch_to_view(a->disp, VIEW_ABOUT); 
        return true;
    } 

    // Захватываем левую и правую кнопки из карусели 
    if (ev == EVENT_SHOW_REFS) {
        view_dispatcher_switch_to_view(a->disp, VIEW_REFS); 
        return true;
    }
    if (ev == EVENT_SHOW_THANKS) {
        view_dispatcher_switch_to_view(a->disp, VIEW_THANKS); 
        return true;
    }

    return false;
}

int32_t flipper_elements_app(void* p) {
    UNUSED(p);

    App* a = malloc(sizeof(App)); memset(a, 0, sizeof(App)); 

    a->gui = furi_record_open(RECORD_GUI);
    a->disp = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(a->disp, a->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(a->disp, a);
    view_dispatcher_set_custom_event_callback(a->disp, main_ev); 

    a->t_splash = furi_timer_alloc(s_t_cb, FuriTimerTypeOnce, a);
    a->v_splash = view_alloc(); view_set_context(a->v_splash, a); 
    view_set_draw_callback(a->v_splash, start_splash_draw); view_set_enter_callback(a->v_splash, s_en_cb);
    view_dispatcher_add_view(a->disp, VIEW_SPLASH, a->v_splash);

    a->v_grid = view_alloc(); view_set_context(a->v_grid, a); 
    view_allocate_model(a->v_grid, ViewModelTypeLocking, sizeof(ViewModel));
    with_view_model(a->v_grid, ViewModel* m, { m->cur_x=0; m->cur_y=0; }, false); 
    view_set_draw_callback(a->v_grid, grid_draw); view_set_input_callback(a->v_grid, grid_input);
    view_set_previous_callback(a->v_grid, grid_b); 
    view_dispatcher_add_view(a->disp, VIEW_GRID, a->v_grid);

    a->v_details = view_alloc(); view_set_context(a->v_details, a);
    view_allocate_model(a->v_details, ViewModelTypeLocking, sizeof(DetModel)); 
    view_set_draw_callback(a->v_details, det_draw); view_set_input_callback(a->v_details, det_input);
    view_set_previous_callback(a->v_details, to_grid);
    view_dispatcher_add_view(a->disp, VIEW_DETAILS, a->v_details);

    // Новое кастомное About-окошко
    a->v_about = view_alloc(); view_set_context(a->v_about, a);
    view_allocate_model(a->v_about, ViewModelTypeLocking, sizeof(AboutModel)); 
    view_set_draw_callback(a->v_about, about_draw); view_set_input_callback(a->v_about, about_input);
    view_set_previous_callback(a->v_about, to_grid);
    view_dispatcher_add_view(a->disp, VIEW_ABOUT, a->v_about);

    // Экран Refs (Открывается кнопкой Влево)
    a->v_refs = text_box_alloc(); text_box_set_font(a->v_refs, TextBoxFontText);
    text_box_set_text(a->v_refs, 
        "Mass/Iso: CIAAW, NUBASE\n"
        "Radii: Slater, Cordero,\n"
        "Shannon, Kaye & Laby\n"
        "Oxid: Greenwood, Earnshaw\n"
        "Polar: Schwerdtfeger & Nagle\n"
        "Phys: RSC, NIST, PubChem");
    View* v_rf = text_box_get_view(a->v_refs); 
    view_set_previous_callback(v_rf, to_about);
    view_dispatcher_add_view(a->disp, VIEW_REFS, v_rf);

    // Экран Thanks (Открывается кнопкой Вправо)
    a->v_thanks = text_box_alloc(); text_box_set_font(a->v_thanks, TextBoxFontText);
    text_box_set_text(a->v_thanks, 
        "Special Thanks to:\n\n"
        "Flipper Zero team &\n"
        "Pavel Zhovner for\n"
        "swiftly providing lab\n"
        "equipment: FZ, VGM,\n"
        "WiFi Devboard &\n"
        "Prototyping boards!");
    View* v_th = text_box_get_view(a->v_thanks); 
    view_set_previous_callback(v_th, to_about);
    view_dispatcher_add_view(a->disp, VIEW_THANKS, v_th);

    a->dialog = dialog_ex_alloc(); dialog_ex_set_context(a->dialog, a);
    dialog_ex_set_header(a->dialog, "Exit Application?", 64, 18, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(a->dialog, "No"); dialog_ex_set_right_button_text(a->dialog, "Yes");
    dialog_ex_set_result_callback(a->dialog, diag_res_cb);
    view_dispatcher_add_view(a->disp, VIEW_EXIT_PROMPT, dialog_ex_get_view(a->dialog));

    a->t_exit = furi_timer_alloc(e_t_cb, FuriTimerTypeOnce, a); a->v_exit_splash = view_alloc();
    view_set_context(a->v_exit_splash, a); view_set_draw_callback(a->v_exit_splash, exit_splash_draw);
    view_set_enter_callback(a->v_exit_splash, e_en_cb); view_dispatcher_add_view(a->disp, VIEW_EXIT_SPLASH, a->v_exit_splash);

    view_dispatcher_switch_to_view(a->disp, VIEW_SPLASH);
    view_dispatcher_run(a->disp);

    view_dispatcher_remove_view(a->disp, VIEW_SPLASH); view_dispatcher_remove_view(a->disp, VIEW_GRID);
    view_dispatcher_remove_view(a->disp, VIEW_DETAILS); view_dispatcher_remove_view(a->disp, VIEW_ABOUT);
    view_dispatcher_remove_view(a->disp, VIEW_REFS); view_dispatcher_remove_view(a->disp, VIEW_THANKS);
    view_dispatcher_remove_view(a->disp, VIEW_EXIT_PROMPT); view_dispatcher_remove_view(a->disp, VIEW_EXIT_SPLASH);

    view_free(a->v_splash); view_free(a->v_grid); view_free(a->v_details); view_free(a->v_exit_splash);
    view_free(a->v_about); text_box_free(a->v_refs); text_box_free(a->v_thanks); dialog_ex_free(a->dialog); 
    furi_timer_free(a->t_splash); furi_timer_free(a->t_exit);
    view_dispatcher_free(a->disp); furi_record_close(RECORD_GUI); free(a); 

    return 0;
}
