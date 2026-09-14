#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <gui/modules/dialog_ex.h> 
#include <gui/view.h>
#include <stdio.h>
#include <string.h>

#define VIEW_SPLASH 0
#define VIEW_GRID 1
#define VIEW_DETAILS 2
#define VIEW_ABOUT 3 
#define VIEW_EXIT_PROMPT 4 
#define VIEW_EXIT_SPLASH 5

#define EVENT_START_APP 101 
#define EVENT_SHOW_DETAILS 102
#define EVENT_SHOW_ABOUT 103
#define EVENT_FINAL_CLOSE 104

#define E(x, y, num, p, g, sym, n, m, mp, bp, d, c, e, af, pl, ie, di, bk, ox, ra, rc, ri, mg, cr, ca) \
    { x, y, num, p, g, sym, n, m, mp, bp, d, c, e, af, pl, ie, di, bk, ox, ra, rc, ri, mg, cr, ca }

typedef struct {
    uint8_t col, row, z, period, group;
    const char* symbol; const char* name; const char* mass; const char* melt; 
    const char* boil; const char* dens; const char* cat; const char* e_conf; 
    const char* aff; const char* pauling; const char* ie; const char* disc; 
    const char* blk; const char* ox; const char* r_a; const char* r_c; 
    const char* r_i; const char* mag; const char* cryst; const char* cas;     
} Element;

const Element DB[] = {
E(0,0,1,1,1,"H","Hydrogen","1.008","13.9","20.2","8.9e-5","diatomic nonmetal","1s1","72.8","2.2","1312","H. Cavendish","s","-1, 1","25","32","139.9 (H-)","diamagnetic","mol. hcp","12385-13-6"),
E(17,0,2,1,18,"He","Helium","4.003","0.9","4.2","1.8e-4","noble gas","1s2","-48.0","N/A","2372, 5250","P. Janssen","s","N/A","N/A","28","N/A","diamagnetic","hcp","7440-59-7"),
E(0,1,3,2,1,"Li","Lithium","6.94","453.6","1603","0.534","alkali metal","[He] 2s1","59.6","0.98","520, 7298","J. Arfwedson","s","1","145","133","76.0 (Li+)","paramagnetic","bcc","7439-93-2"),
E(1,1,4,2,2,"Be","Beryllium","9.012","1560","2742","1.85","alkaline earth","[He] 2s2","-48.0","1.57","899, 1757","L. Vauquelin","s","2","105","102","45.0 (Be2+)","diamagnetic","hcp","7440-41-7"),
E(12,1,5,2,13,"B","Boron","10.81","2349","4200","2.34","metalloid","[He] 2s2 2p1","27.0","2.04","800, 2427","J. Gay-Lussac","p","3","85","85","27.0 (B3+)","diamagnetic","rhombohedral","7440-42-8"),
E(13,1,6,2,14,"C","Carbon","12.01","N/A","N/A","2.267","poly nonmetal","[He] 2s2 2p2","121.8","2.55","1086, 2352","Ancient Egypt","p","-4, 4","70","75","16.0 (C4+)","diamagnetic","hex. graphite","7440-44-0"),
E(14,1,7,2,15,"N","Nitrogen","14.00","63.1","77.3","1.2e-3","dia nonmetal","[He] 2s2 2p3","-6.8","3.04","1402, 2856","D. Rutherford","p","-3, 3, 5","65","71","146.0 (N3-)","diamagnetic","mol. cubic","7727-37-9"),
E(15,1,8,2,16,"O","Oxygen","15.99","54.3","90.1","1.4e-3","dia nonmetal","[He] 2s2 2p4","141.0","3.44","1313, 3388","C. Scheele","p","-2","60","63","140.0 (O2-)","paramagnetic","mol. monoclin.","7782-44-7"),
E(16,1,9,2,17,"F","Fluorine","18.99","53.4","85.0","1.7e-3","dia nonmetal","[He] 2s2 2p5","328.1","3.98","1681, 3374","A. Ampere","p","-1","50","64","133.0 (F-)","diamagnetic","mol. monoclin.","7782-41-4"),
E(17,1,10,2,18,"Ne","Neon","20.18","24.5","27.1","9.0e-4","noble gas","[He] 2s2 2p6","-116","N/A","2080, 3952","M. Travers","p","N/A","N/A","67","N/A","diamagnetic","fcc","7440-01-9"),
E(0,2,11,3,1,"Na","Sodium","22.98","370.9","1156","0.968","alkali metal","[Ne] 3s1","52.8","0.93","495, 4562","H. Davy","s","1","180","155","102.0 (Na+)","paramagnetic","bcc","7440-23-5"),
E(1,2,12,3,2,"Mg","Magnesium","24.30","923","1363","1.738","alkaline earth","[Ne] 3s2","-40.0","1.31","737, 1450","J. Black","s","2","150","139","72.0 (Mg2+)","paramagnetic","hcp","7439-95-4"),
E(12,2,13,3,13,"Al","Aluminium","26.98","933.4","2743","2.7","post-transition","[Ne] 3s2 3p1","41.7","1.61","577, 1816","H. Davy","p","3","125","126","53.5 (Al3+)","paramagnetic","fcc","7429-90-5"),
E(13,2,14,3,14,"Si","Silicon","28.08","1687","3538","2.33","metalloid","[Ne] 3s2 3p2","134.0","1.90","786, 1577","J. Berzelius","p","-4, 4","110","116","40.0 (Si4+)","diamagnetic","diamond cubic","7440-21-3"),
E(14,2,15,3,15,"P","Phosphorus","30.97","N/A","N/A","1.823","poly nonmetal","[Ne] 3s2 3p3","72.0","2.19","1011, 1907","H. Brand","p","-3, 3, 5","100","111","212.0 (P3-)","diamagnetic","orthorhombic","7723-14-0"),
E(15,2,16,3,16,"S","Sulfur","32.06","388.3","717.8","2.08","poly nonmetal","[Ne] 3s2 3p4","200.4","2.58","999, 2252","Ancient china","p","-2, 2, 4, 6","100","103","184.0 (S2-)","diamagnetic","orthorhombic","7704-34-9"),
E(16,2,17,3,17,"Cl","Chlorine","35.45","171.6","239.1","3.2e-3","dia nonmetal","[Ne] 3s2 3p5","348.5","3.16","1251, 2298","C. Scheele","p","-1, 1, 3, 5, 7","100","99","181.0 (Cl-)","diamagnetic","mol. orthorho.","7782-50-5"),
E(17,2,18,3,18,"Ar","Argon","39.94","83.8","87.3","1.7e-3","noble gas","[Ne] 3s2 3p6","-96.0","N/A","1520, 2665","Lord Rayleigh","p","N/A","N/A","96","N/A","diamagnetic","fcc","7440-37-1"),
E(0,3,19,4,1,"K","Potassium","39.09","336.7","1032","0.89","alkali metal","[Ar] 4s1","48.3","0.82","418, 3052","H. Davy","s","1","220","196","138.0 (K+)","paramagnetic","bcc","7440-09-7"),
E(1,3,20,4,2,"Ca","Calcium","40.07","1115","1757","1.55","alkaline earth","[Ar] 4s2","2.37","1.00","589, 1145","H. Davy","s","2","180","171","100.0 (Ca2+)","paramagnetic","fcc","7440-70-2"),
E(2,3,21,4,3,"Sc","Scandium","44.95","1814","3109","2.985","transition metal","[Ar] 3d1 4s2","18.0","1.36","633, 1235","L. Nilson","d","3","160","148","74.5 (Sc3+)","paramagnetic","hcp","7440-20-2"),
E(3,3,22,4,4,"Ti","Titanium","47.86","1941","3560","4.506","transition metal","[Ar] 3d2 4s2","7.28","1.54","658, 1309","W. Gregor","d","4","140","136","60.5 (Ti4+)","paramagnetic","hcp","7440-32-6"),
E(4,3,23,4,5,"V","Vanadium","50.94","2183","3680","6.11","transition metal","[Ar] 3d3 4s2","50.9","1.63","650, 1414","A. del Rio","d","5","135","134","54.0 (V5+)","paramagnetic","bcc","7440-62-2"),
E(5,3,24,4,6,"Cr","Chromium","51.99","2180","2944","7.15","transition metal","[Ar] 3d5 4s1","65.2","1.66","652, 1590","L. Vauquelin","d","3, 6","140","122","61.5 (Cr3+)","antiferromag.","bcc","7440-47-3"),
E(6,3,25,4,7,"Mn","Manganese","54.93","1519","2334","7.21","transition metal","[Ar] 3d5 4s2","-50.0","1.55","717, 1509","T. Bergman","d","2, 4, 7","140","119","83.0 (Mn2+)","complex order","comp. cubic","7439-96-5"),
E(7,3,26,4,8,"Fe","Iron","55.84","1811","3134","7.86","transition metal","[Ar] 3d6 4s2","14.7","1.83","762, 1561","5000 BC","d","2, 3","140","116","64.5 (Fe3+)","ferromagnetic","bcc","7439-89-6"),
E(8,3,27,4,9,"Co","Cobalt","58.93","1768","3200","8.9","transition metal","[Ar] 3d7 4s2","63.8","1.88","760, 1648","G. Brandt","d","2, 3","135","111","61.0 (Co3+)","ferromagnetic","hcp","7440-48-4"),
E(9,3,28,4,10,"Ni","Nickel","58.69","1728","3003","8.908","transition metal","[Ar] 3d8 4s2","111.6","1.91","737, 1753","A. Cronstedt","d","2","135","110","69.0 (Ni2+)","ferromagnetic","fcc","7440-02-0"),
E(10,3,29,4,11,"Cu","Copper","63.54","1357","2835","8.96","transition metal","[Ar] 3d10 4s1","119.2","1.90","745, 1957","Middle East","d","2","135","112","73.0 (Cu2+)","diamagnetic","fcc","7440-50-8"),
E(11,3,30,4,12,"Zn","Zinc","65.38","692.6","1180","7.14","transition metal","[Ar] 3d10 4s2","-58.0","1.65","906, 1733","India","d","2","135","118","74.0 (Zn2+)","diamagnetic","hcp","7440-66-6"),
E(12,3,31,4,13,"Ga","Gallium","69.72","302.9","2673","5.91","post-transition","[Ar] 3d10 4s2 4p1","41.0","1.81","578, 1979","L. Boisbaudran","p","3","130","124","62.0 (Ga3+)","diamagnetic","orthorhombic","7440-55-3"),
E(13,3,32,4,14,"Ge","Germanium","72.63","1211","3106","5.323","metalloid","[Ar] 3d10 4s2 4p2","118.9","2.01","762, 1537","C. Winkler","p","-4, 4","125","121","53.0 (Ge4+)","diamagnetic","diamond cubic","7440-56-4"),
E(14,3,33,4,15,"As","Arsenic","74.92","N/A","N/A","5.727","metalloid","[Ar] 3d10 4s2 4p3","77.6","2.18","947, 1798","Bronze Age","p","-3, 3, 5","115","121","58.0 (As3+)","diamagnetic","rhombohedral","7440-38-2"),
E(15,3,34,4,16,"Se","Selenium","78.97","494.0","958","4.81","poly nonmetal","[Ar] 3d10 4s2 4p4","194.9","2.55","941, 2045","J. Berzelius","p","-2, 2, 4, 6","115","116","198.0 (Se2-)","diamagnetic","trig. helical","7782-49-2"),
E(16,3,35,4,17,"Br","Bromine","79.90","265.8","332","3.103","dia nonmetal","[Ar] 3d10 4s2 4p5","324.5","2.96","1139, 2103","A. Balard","p","-1, 1, 3, 5, 7","115","114","196.0 (Br-)","diamagnetic","mol. orthorho.","7726-95-6"),
E(17,3,36,4,18,"Kr","Krypton","83.79","115.7","119.9","3.7e-3","noble gas","[Ar] 3d10 4s2 4p6","-96.0","3.00","1350, 2350","W. Ramsay","p","N/A","N/A","117","N/A","diamagnetic","fcc","7439-90-9"),
E(0,4,37,5,1,"Rb","Rubidium","85.46","312.4","961","1.532","alkali metal","[Kr] 5s1","46.8","0.82","403, 2633","R. Bunsen","s","1","235","210","152.0 (Rb+)","paramagnetic","bcc","7440-17-7"),
E(1,4,38,5,2,"Sr","Strontium","87.62","1050","1650","2.64","alkaline earth","[Kr] 5s2","5.0","0.95","549, 1064","W. Cruickshank","s","2","200","185","118.0 (Sr2+)","paramagnetic","fcc","7440-24-6"),
E(2,4,39,5,3,"Y","Yttrium","88.90","1799","3203","4.472","transition metal","[Kr] 4d1 5s2","29.6","1.22","600, 1180","J. Gadolin","d","3","180","163","90.0 (Y3+)","paramagnetic","hcp","7440-65-5"),
E(3,4,40,5,4,"Zr","Zirconium","91.22","2128","4650","6.52","transition metal","[Kr] 4d2 5s2","41.8","1.33","640, 1270","M. Klaproth","d","4","155","154","72.0 (Zr4+)","paramagnetic","hcp","7440-67-7"),
E(4,4,41,5,5,"Nb","Niobium","92.90","2750","5017","8.57","transition metal","[Kr] 4d4 5s1","88.5","1.60","652, 1380","C. Hatchett","d","5","145","147","64.0 (Nb5+)","paramagnetic","bcc","7440-03-1"),
E(5,4,42,5,6,"Mo","Molybdenum","95.95","2896","4912","10.28","transition metal","[Kr] 4d5 5s1","72.1","2.16","684, 1560","C. Scheele","d","4, 6","145","138","59.0 (Mo6+)","paramagnetic","bcc","7439-98-7"),
E(6,4,43,5,7,"Tc","Technetium","[98]","2430","4538","11.0","transition metal","[Kr] 4d5 5s2","53.0","1.90","702, 1470","E. Segre","d","1...7","135","128","56.0 (Tc7+)","paramagnetic","hcp","7440-26-8"),
E(7,4,44,5,8,"Ru","Ruthenium","101.07","2607","4423","12.45","transition metal","[Kr] 4d7 5s1","100.9","2.20","710, 1620","K. Claus","d","-2...8","130","125","68.0 (Ru3+)","paramagnetic","hcp","7440-18-8"),
E(8,4,45,5,9,"Rh","Rhodium","102.90","2237","3968","12.41","transition metal","[Kr] 4d8 5s1","110.2","2.28","719, 1740","W. Wollaston","d","-1,1,2,3,4,5,6","135","125","66.5 (Rh3+)","paramagnetic","fcc","7440-16-6"),
E(9,4,46,5,10,"Pd","Palladium","106.42","1828","3236","12.023","transition metal","[Kr] 4d10","54.2","2.20","804, 1870","W. Wollaston","d","2, 4","140","120","86.0 (Pd2+)","paramagnetic","fcc","7440-05-3"),
E(10,4,47,5,11,"Ag","Silver","107.86","1234","2435","10.49","transition metal","[Kr] 4d10 5s1","125.8","1.93","731, 2070","before 5000 BC","d","1","160","128","115.0 (Ag+)","diamagnetic","fcc","7440-22-4"),
E(11,4,48,5,12,"Cd","Cadmium","112.41","594.2","1040","8.65","transition metal","[Kr] 4d10 5s2","-68.0","1.69","867, 1631","K. Hermann","d","2","155","136","95.0 (Cd2+)","diamagnetic","hcp","7440-43-9"),
E(12,4,49,5,13,"In","Indium","114.81","429.7","2345","7.31","post-transition","[Kr] 4d10 5s2 5p1","37.0","1.78","558, 1820","F. Reich","p","3","155","142","80.0 (In3+)","diamagnetic","bc-tetrag.","7440-74-6"),
E(13,4,50,5,14,"Sn","Tin","118.71","505.0","2875","7.265","post-transition","[Kr] 4d10 5s2 5p2","107.2","1.96","708, 1411","before 3500 BC","p","-4, 2, 4","145","140","69.0 (Sn4+)","diamagnetic","bc-tetra(beta)","7440-31-5"),
E(14,4,51,5,15,"Sb","Antimony","121.76","903.7","1908","6.697","metalloid","[Kr] 4d10 5s2 5p3","101.0","2.05","834, 1594","before 3000 BC","p","-3, 3, 5","145","140","76.0 (Sb3+)","diamagnetic","rhombohedral","7440-36-0"),
E(15,4,52,5,16,"Te","Tellurium","127.60","722.6","1261","6.24","metalloid","[Kr] 4d10 5s2 5p4","190.1","2.10","869, 1790","Reichenstein","p","-2, 2, 4, 6","140","136","221.0 (Te2-)","diamagnetic","trig. helical","13494-80-7"),
E(16,4,53,5,17,"I","Iodine","126.90","386.8","457.4","4.933","dia nonmetal","[Kr] 4d10 5s2 5p5","295.1","2.66","1008, 1845","B. Courtois","p","-1, 1, 3, 5, 7","140","133","220.0 (I-)","diamagnetic","mol. orthorho.","7553-56-2"),
E(17,4,54,5,18,"Xe","Xenon","131.29","161.4","165.0","5.8e-3","noble gas","[Kr] 4d10 5s2 5p6","-77.0","2.60","1170, 2046","W. Ramsay","p","N/A","N/A","131","N/A","diamagnetic","fcc","7440-63-3"),
E(0,5,55,6,1,"Cs","Cesium","132.90","301.7","944","1.93","alkali metal","[Xe] 6s1","45.5","0.79","375, 2234","R. Bunsen","s","1","260","232","167.0 (Cs+)","paramagnetic","bcc","7440-46-2"),
E(1,5,56,6,2,"Ba","Barium","137.32","1000","2118","3.51","alkaline earth","[Xe] 6s2","13.9","0.89","502, 965","C. Scheele","s","2","215","196","135.0 (Ba2+)","paramagnetic","bcc","7440-39-3"),
E(2,8,57,6,3,"La","Lanthanum","138.90","1193","3737","6.162","lanthanide","[Xe] 5d1 6s2","53.0","1.10","538, 1067","C. Mosander","f","3","195","180","103.2 (La3+)","paramagnetic","double hcp","7439-91-0"),
E(3,8,58,6,3,"Ce","Cerium","140.11","1068","3716","6.77","lanthanide","[Xe] 4f1 5d1 6s2","55.0","1.12","534, 1050","M. Klaproth","f","3, 4","185","163","101.0 (Ce3+)","paramagnetic","fcc(gamma)","7440-45-1"),
E(4,8,59,6,3,"Pr","Praseodymium","140.90","1208","3403","6.77","lanthanide","[Xe] 4f3 6s2","93.0","1.13","527, 1020","C. Welsbach","f","3, 4","185","176","99.0 (Pr3+)","paramagnetic","double hcp","7440-10-0"),
E(5,8,60,6,3,"Nd","Neodymium","144.24","1297","3347","7.01","lanthanide","[Xe] 4f4 6s2","184.8","1.14","533, 1040","C. Welsbach","f","3","185","174","98.3 (Nd3+)","paramagnetic","double hcp","7440-00-8"),
E(6,8,61,6,3,"Pm","Promethium","[145]","1315","3273","7.26","lanthanide","[Xe] 4f5 6s2","12.4","1.13","540, 1050","C. Wu","f","3","185","173","97.0 (Pm3+)","paramagnetic","double hcp","7440-12-2"),
E(7,8,62,6,3,"Sm","Samarium","150.36","1345","2173","7.52","lanthanide","[Xe] 4f6 6s2","15.6","1.17","544, 1070","L. Boisbaudran","f","2, 3","185","172","95.8 (Sm3+)","paramagnetic","rhombohedral","7440-19-9"),
E(8,8,63,6,3,"Eu","Europium","151.96","1099","1802","5.244","lanthanide","[Xe] 4f7 6s2","11.2","1.20","547, 1085","E. Demarcay","f","2, 3","185","168","94.7 (Eu3+)","paramagnetic","bcc","7440-53-1"),
E(9,8,64,6,3,"Gd","Gadolinium","157.25","1585","3273","7.90","lanthanide","[Xe] 4f7 5d1 6s2","13.2","1.20","593, 1170","J. Marignac","f","3","180","169","93.5 (Gd3+)","ferromagnetic","hcp","7440-54-2"),
E(10,8,65,6,3,"Tb","Terbium","158.92","1629","3396","8.23","lanthanide","[Xe] 4f9 6s2","112.4","1.10","565, 1110","C. Mosander","f","3, 4","175","168","92.3 (Tb3+)","paramagnetic","hcp","7440-27-9"),
E(11,8,66,6,3,"Dy","Dysprosium","162.50","1680","2840","8.54","lanthanide","[Xe] 4f10 6s2","33.9","1.22","573, 1130","L. Boisbaudran","f","3","175","167","91.2 (Dy3+)","paramagnetic","hcp","7429-91-6"),
E(12,8,67,6,3,"Ho","Holmium","164.93","1734","2873","8.79","lanthanide","[Xe] 4f11 6s2","32.6","1.23","581, 1140","Delafontaine","f","3","175","166","90.1 (Ho3+)","paramagnetic","hcp","7440-60-0"),
E(13,8,68,6,3,"Er","Erbium","167.25","1802","3141","9.066","lanthanide","[Xe] 4f12 6s2","30.1","1.24","589, 1150","C. Mosander","f","3","175","165","89.0 (Er3+)","paramagnetic","hcp","7440-52-0"),
E(14,8,69,6,3,"Tm","Thulium","168.93","1818","2223","9.32","lanthanide","[Xe] 4f13 6s2","99.0","1.25","596, 1160","P. Cleve","f","3","175","164","88.0 (Tm3+)","paramagnetic","hcp","7440-30-4"),
E(15,8,70,6,3,"Yb","Ytterbium","173.04","1097","1469","6.90","lanthanide","[Xe] 4f14 6s2","-1.9","1.10","603, 1174","J. Marignac","f","2, 3","175","170","86.8 (Yb3+)","paramagnetic","fcc","7440-64-4"),
E(16,8,71,6,3,"Lu","Lutetium","174.96","1925","3675","9.841","lanthanide","[Xe] 4f14 5d1 6s2","33.4","1.27","523, 1340","G. Urbain","d","3","175","162","86.1 (Lu3+)","paramagnetic","hcp","7439-94-3"),
E(3,5,72,6,4,"Hf","Hafnium","178.49","2506","4876","13.31","transition metal","[Xe] 4f14 5d2 6s2","17.1","1.30","658, 1440","D. Coster","d","4","155","152","71.0 (Hf4+)","paramagnetic","hcp","7440-58-6"),
E(4,5,73,6,5,"Ta","Tantalum","180.94","3290","5731","16.69","transition metal","[Xe] 4f14 5d3 6s2","31.0","1.50","761, 1500","A. Ekeberg","d","5","145","146","64.0 (Ta5+)","paramagnetic","bcc","7440-25-7"),
E(5,5,74,6,6,"W","Tungsten","183.84","3695","6203","19.25","transition metal","[Xe] 4f14 5d4 6s2","78.7","2.36","770, 1700","C. Scheele","d","6","135","137","60.0 (W6+)","paramagnetic","bcc","7440-33-7"),
E(6,5,75,6,7,"Re","Rhenium","186.20","3459","5869","21.02","transition metal","[Xe] 4f14 5d5 6s2","5.8","1.90","760, 1260","M. Ogawa","d","-1...7","135","131","53.0 (Re7+)","paramagnetic","hcp","7440-15-5"),
E(7,5,76,6,8,"Os","Osmium","190.23","3306","5285","22.59","transition metal","[Xe] 4f14 5d6 6s2","103.9","2.20","840, 1600","S. Tennant","d","-2...8","130","129","63.0 (Os4+)","paramagnetic","hcp","7440-04-2"),
E(8,5,77,6,9,"Ir","Iridium","192.21","2719","4403","22.56","transition metal","[Xe] 4f14 5d7 6s2","150.9","2.20","880, 1600","S. Tennant","d","-1,1...6","135","122","68.0 (Ir3+)","paramagnetic","fcc","7439-88-5"),
E(9,5,78,6,10,"Pt","Platinum","195.08","2041","4098","21.45","transition metal","[Xe] 4f14 5d9 6s1","205.0","2.28","870, 1791","A. Ulloa","d","2, 4","135","123","80.0 (Pt2+)","paramagnetic","fcc","7440-06-4"),
E(10,5,79,6,11,"Au","Gold","196.96","1337","3243","19.3","transition metal","[Xe] 4f14 5d10 6s1","222.7","2.54","890, 1980","Middle East","d","3","135","124","85.0 (Au3+)","diamagnetic","fcc","7440-57-5"),
E(11,5,80,6,12,"Hg","Mercury","200.59","234.3","629.8","13.534","transition metal","[Xe] 4f14 5d10 6s2","-48.0","2.00","1007, 1810","2000 BCE","d","2","150","133","102.0 (Hg2+)","diamagnetic","rhombohedral","7439-97-6"),
E(12,5,81,6,13,"Tl","Thallium","204.38","577","1746","11.85","post-transition","[Xe] 4f14 5d10 6s2 6p1","36.4","1.62","589, 1971","W. Crookes","p","1, 3","190","144","150.0 (Tl+)","diamagnetic","hcp","7440-28-0"),
E(13,5,82,6,14,"Pb","Lead","207.21","600.6","2022","11.34","post-transition","[Xe] 4f14 5d10 6s2 6p2","34.4","1.87","715, 1450","Middle East","p","2, 4","180","144","119.0 (Pb2+)","diamagnetic","fcc","7439-92-1"),
E(14,5,83,6,15,"Bi","Bismuth","208.98","544.7","1837","9.78","post-transition","[Xe] 4f14 5d10 6s2 6p3","90.9","2.02","703, 1610","C. Geoffroy","p","3, 5","160","151","103.0 (Bi3+)","diamagnetic","rhombohedral","7440-69-9"),
E(15,5,84,6,16,"Po","Polonium","[209]","527","1235","9.196","post-transition","[Xe] 4f14 5d10 6s2 6p4","136.0","2.00","812.1","P. Curie","p","2, 4, 6","190","145","94.0 (Po4+)","diamagnetic","simple cubic","7440-08-6"),
E(16,5,85,6,17,"At","Astatine","[210]","575","610","7.0","metalloid","[Xe] 4f14 5d10 6s2 6p5","233.0","2.20","899.0","D. Corson","p","-1, 1, 3, 5, 7","N/A","147","62.0 (At-)","N/A","N/A","7440-68-8"),
E(17,5,86,6,18,"Rn","Radon","[222]","202","211.5","9.7e-3","noble gas","[Xe] 4f14 5d10 6s2 6p6","-68.0","2.20","1037","F. Dorn","p","N/A","N/A","142","N/A","diamagnetic","fcc","10043-92-2"),
E(0,6,87,7,1,"Fr","Francium","[223]","300","950","2.48","alkali metal","[Rn] 7s1","46.8","0.79","380.0","M. Perey","s","1","N/A","223","180.0 (Fr+)","N/A","N/A","7440-73-5"),
E(1,6,88,7,2,"Ra","Radium","[226]","1233","2010","5.5","alkaline earth","[Rn] 7s2","9.6","0.90","509, 979","P. Curie","s","2","215","201","148.0 (Ra2+)","N/A","bcc","7440-14-4"),
E(2,9,89,7,3,"Ac","Actinium","[227]","1500","3500","10.0","actinide","[Rn] 6d1 7s2","33.7","1.10","499, 1170","F. Giesel","f","3","195","186","106.5 (Ac3+)","N/A","fcc","7440-34-8"),
E(3,9,90,7,3,"Th","Thorium","[232]","2023","5061","11.7","actinide","[Rn] 6d2 7s2","112.7","1.30","587, 1110","J. Berzelius","f","4","180","175","94.0 (Th4+)","paramagnetic","fcc","7440-29-1"),
E(4,9,91,7,3,"Pa","Protactinium","[231]","1841","4300","15.37","actinide","[Rn] 5f2 6d1 7s2","53.0","1.50","568.0","W. Crookes","f","5","180","169","78.0 (Pa5+)","paramagnetic","bc-tetragonal","7440-13-3"),
E(5,9,92,7,3,"U","Uranium","[238]","1405","4404","19.1","actinide","[Rn] 5f3 6d1 7s2","50.9","1.38","597, 1420","M. Klaproth","f","3, 4, 5, 6","175","170","73.0 (U6+)","paramagnetic","orthorhombic","7440-61-1"),
E(6,9,93,7,3,"Np","Neptunium","[237]","912","4447","20.2","actinide","[Rn] 5f4 6d1 7s2","45.8","1.36","604.5","E. McMillan","f","3...7","175","171","71.0 (Np7+)","paramagnetic","orthorhombic","7439-99-8"),
E(7,9,94,7,3,"Pu","Plutonium","[244]","912.5","3505","19.816","actinide","[Rn] 5f6 7s2","-48.3","1.28","584.7","G. Seaborg","f","3...7","175","172","86.0 (Pu4+)","paramagnetic","monoclinic","7440-07-5"),
E(8,9,95,7,3,"Am","Americium","[243]","1449","2880","12.0","actinide","[Rn] 5f7 7s2","9.9","1.13","578.0","G. Seaborg","f","3, 4, 5, 6","175","166","97.5 (Am3+)","paramagnetic","double hcp","7440-35-9"),
E(9,9,96,7,3,"Cm","Curium","[247]","1613","3383","13.51","actinide","[Rn] 5f7 6d1 7s2","27.1","1.28","581.0","G. Seaborg","f","3","N/A","166","97.0 (Cm3+)","paramagnetic","double hcp","7440-51-9"),
E(10,9,97,7,3,"Bk","Berkelium","[247]","1259","2900","14.78","actinide","[Rn] 5f9 7s2","-165.2","1.30","601.0","LBNL","f","3, 4","N/A","168","96.0 (Bk3+)","paramagnetic","double hcp","7440-40-6"),
E(11,9,98,7,3,"Cf","Californium","[251]","1173","1743","15.1","actinide","[Rn] 5f10 7s2","-97.3","1.30","608.0","LBNL","f","2, 3, 4","N/A","168","95.0 (Cf3+)","paramagnetic","double hcp","7440-71-3"),
E(12,9,99,7,3,"Es","Einsteinium","[252]","1133","1269","8.84","actinide","[Rn] 5f11 7s2","-28.6","1.30","619.0","LBNL","f","3","N/A","165","83.5 (Es3+)","N/A","fcc","7429-92-7"),
E(13,9,100,7,3,"Fm","Fermium","[257]","1800","N/A","N/A","actinide","[Rn] 5f12 7s2","33.9","1.30","627.0","LBNL","f","3","N/A","167","N/A","N/A","N/A","7440-72-4"),
E(14,9,101,7,3,"Md","Mendelevium","[258]","1100","N/A","N/A","actinide","[Rn] 5f13 7s2","93.9","1.30","635.0","LBNL","f","2, 3","N/A","173","N/A","N/A","N/A","7440-03-1"),
E(15,9,102,7,3,"No","Nobelium","[259]","1100","N/A","N/A","actinide","[Rn] 5f14 7s2","-223.2","1.30","642.0","JINR","f","2, 3","N/A","176","N/A","N/A","N/A","7440-05-3"),
E(16,9,103,7,3,"Lr","Lawrencium","[266]","1900","N/A","N/A","actinide","[Rn] 5f14 7s2 7p1","-30.0","1.30","470.0","LBNL","d","3","N/A","161","N/A","N/A","N/A","22537-19-3"),
E(3,6,104,7,4,"Rf","Rutherfordium","[267]","2400","5800","N/A","transition metal","[Rn] 5f14 6d2 7s2","N/A","N/A","580.0","JINR","d","4","N/A","157","N/A","N/A","N/A","53850-35-4"),
E(4,6,105,7,5,"Db","Dubnium","[268]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d3 7s2","N/A","N/A","N/A","JINR","d","5","N/A","149","N/A","N/A","N/A","53850-36-5"),
E(5,6,106,7,6,"Sg","Seaborgium","[269]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d4 7s2","N/A","N/A","N/A","LBNL","d","6","N/A","143","N/A","N/A","N/A","54038-81-2"),
E(6,6,107,7,7,"Bh","Bohrium","[270]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d5 7s2","N/A","N/A","N/A","GSI","d","7","N/A","141","N/A","N/A","N/A","54037-14-8"),
E(7,6,108,7,8,"Hs","Hassium","[269]","126","N/A","N/A","transition metal","*[Rn] 5f14 6d6 7s2","N/A","N/A","N/A","GSI","d","8","N/A","134","N/A","N/A","N/A","54037-57-9"),
E(8,6,109,7,9,"Mt","Meitnerium","[278]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d7 7s2","N/A","N/A","N/A","GSI","d","N/A","N/A","129","N/A","N/A","N/A","54038-01-6"),
E(9,6,110,7,10,"Ds","Darmstadtium","[281]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d9 7s1","N/A","N/A","N/A","GSI","d","N/A","N/A","128","N/A","N/A","N/A","54083-77-1"),
E(10,6,111,7,11,"Rg","Roentgenium","[282]","N/A","N/A","N/A","transition metal","*[Rn] 5f14 6d10 7s1","151.0","N/A","N/A","GSI","d","N/A","N/A","121","N/A","N/A","N/A","54084-26-3"),
E(11,6,112,7,12,"Cn","Copernicium","[285]","N/A","3570","N/A","transition metal","*[Rn] 5f14 6d10 7s2","N/A","N/A","N/A","GSI","d","N/A","N/A","122","N/A","N/A","N/A","54084-26-3"),
E(12,6,113,7,13,"Nh","Nihonium","[286]","700","1430","N/A","transition metal","*[Rn] 5f14 6d10 7s2 7p1","66.6","N/A","N/A","RIKEN","p","N/A","N/A","136","N/A","N/A","N/A","N/A"),
E(13,6,114,7,14,"Fl","Flerovium","[289]","340","420","14.0","post-transition","*[Rn] 5f14 6d10 7s2 7p2","N/A","N/A","N/A","JINR","p","N/A","N/A","143","N/A","N/A","N/A","N/A"),
E(14,6,115,7,15,"Mc","Moscovium","[290]","670","1400","N/A","post-transition","*[Rn] 5f14 6d10 7s2 7p3","35.3","N/A","N/A","JINR","p","N/A","N/A","162","N/A","N/A","N/A","N/A"),
E(15,6,116,7,16,"Lv","Livermorium","[293]","709","1085","N/A","post-transition","*[Rn] 5f14 6d10 7s2 7p4","74.9","N/A","N/A","JINR","p","N/A","N/A","175","N/A","N/A","N/A","N/A"),
E(16,6,117,7,17,"Ts","Tennessine","[294]","723","883","N/A","metalloid","*[Rn] 5f14 6d10 7s2 7p5","165.9","N/A","N/A","JINR","p","N/A","N/A","165","N/A","N/A","N/A","N/A"),
E(17,6,118,7,18,"Og","Oganesson","[294]","N/A","350","5.0","noble gas","*[Rn] 5f14 6d10 7s2 7p6","5.4","N/A","N/A","JINR","p","N/A","N/A","157","N/A","N/A","N/A","N/A"),
E(0,7,119,8,1,"Uue","Ununennium","[315]","N/A","630","N/A","alkali metal","*[Uuo] 8s1","63.8","N/A","N/A","GSI","s","N/A","N/A","N/A","N/A","N/A","N/A","N/A")
};

static const Element* get_elm_xy(uint8_t c, uint8_t r) {
    for(uint16_t i=0; i<119; i++) if(DB[i].col == c && DB[i].row == r) return &DB[i];
    return NULL; 
}

static const Element* get_elm_z(uint8_t z) {
    if(z > 0 && z <= 119) return &DB[z-1]; // Прямой доступ к индексу O(1) !
    return NULL;
}

typedef struct { uint8_t cur_x; uint8_t cur_y; } ViewModel;

typedef struct {
    Gui* gui; ViewDispatcher* disp; View* v_splash; View* v_grid;         
    View* v_exit_splash; TextBox* v_details; TextBox* v_about;    
    DialogEx* dialog; FuriTimer* t_splash; FuriTimer* t_exit; char buf[1024]; 
} App;

// ========================= СПЛЕШ: СТАРТ ========================
static void start_splash_draw(Canvas* c, void* m) {
    UNUSED(m);
    canvas_set_color(c, ColorBlack); canvas_draw_box(c, 0, 0, 128, 64); 
    canvas_set_color(c, ColorWhite);   
    canvas_set_font(c, FontPrimary); 
    canvas_draw_str_aligned(c, 64, 15, AlignCenter, AlignCenter, "Periodic Table");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 30, AlignCenter, AlignCenter, "of Elements");
    canvas_draw_str_aligned(c, 64, 52, AlignCenter, AlignCenter, "lab-66@Siarhei Besarab");
}
static void s_t_cb(void* ctx) { view_dispatcher_send_custom_event(((App*)ctx)->disp, EVENT_START_APP); }
static void s_en_cb(void* ctx) { furi_timer_start(((App*)ctx)->t_splash, furi_ms_to_ticks(2000)); }


// ========================= СПЛЕШ: ВЫХОД ========================
static void exit_splash_draw(Canvas* c, void* ctx) {
    UNUSED(ctx);
    canvas_set_color(c, ColorBlack); canvas_draw_box(c, 0, 0, 128, 64);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary); 
    canvas_draw_str_aligned(c, 64, 25, AlignCenter, AlignCenter, "See you later!");
    canvas_set_font(c, FontSecondary); 
    canvas_draw_str_aligned(c, 64, 45, AlignCenter, AlignCenter, "LAB-66 (C) 2026"); 
}
static void e_t_cb(void* ctx) { view_dispatcher_send_custom_event(((App*)ctx)->disp, EVENT_FINAL_CLOSE); }
static void e_en_cb(void* ctx) { furi_timer_start(((App*)ctx)->t_exit, furi_ms_to_ticks(1500)); }

static void diag_res_cb(DialogExResult r, void* ctx) {
    App* app = (App*)ctx;
    if (r == DialogExResultLeft) view_dispatcher_switch_to_view(app->disp, VIEW_GRID);
    else if (r == DialogExResultRight) view_dispatcher_switch_to_view(app->disp, VIEW_EXIT_SPLASH);
}

// ========================== ГЛАВНАЯ СЕТКА ==========================
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
            if (y == model->cur_y && x == model->cur_x) canvas_draw_box(canvas, px, py, s_sz, s_sz);
            else canvas_draw_frame(canvas, px, py, s_sz, s_sz);
        }
    }
    
    canvas_draw_line(canvas, 62, 0, 62, 63); 
    const Element* elm = get_elm_xy(model->cur_x, model->cur_y);
    if (elm) {
        char d[32]; snprintf(d, sizeof(d), "%d", elm->z);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 67, 10, d);
        
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 95, 20, AlignCenter, AlignCenter, elm->symbol);
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 95, 33, AlignCenter, AlignCenter, elm->name);
        
        snprintf(d, sizeof(d), "P: %d | G: %d", elm->period, elm->group);
        canvas_draw_str_aligned(canvas, 95, 45, AlignCenter, AlignCenter, d);
        
        // Удалена лишняя 'u', если это масса
        snprintf(d, sizeof(d), "M: %s", elm->mass);
        canvas_draw_str_aligned(canvas, 95, 57, AlignCenter, AlignCenter, d);
    }
}

static bool grid_input(InputEvent* ev, void* _ctx) {
    App* app = (App*)_ctx; 
    if (ev->key == InputKeyOk) {
        if(ev->type == InputTypeShort) {
            uint8_t tx=0, ty=0;
            with_view_model(app->v_grid, ViewModel* m, { tx=m->cur_x; ty=m->cur_y; }, false);
            if (get_elm_xy(tx, ty)) view_dispatcher_send_custom_event(app->disp, EVENT_SHOW_DETAILS);
            return true;
        } else if (ev->type == InputTypeLong) {
            view_dispatcher_send_custom_event(app->disp, EVENT_SHOW_ABOUT); return true;
        }
    }

    if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
        with_view_model(app->v_grid, ViewModel* m, {
            if (ev->key == InputKeyRight) {
                const Element* cr = get_elm_xy(m->cur_x, m->cur_y);
                if (cr) { const Element* nx = get_elm_z(cr->z + 1); if(nx) { m->cur_x = nx->col; m->cur_y = nx->row; } }
            }
            else if (ev->key == InputKeyLeft) {
                const Element* cr = get_elm_xy(m->cur_x, m->cur_y);
                if (cr) { const Element* pv = get_elm_z(cr->z - 1); if(pv) { m->cur_x = pv->col; m->cur_y = pv->row; } }
            }
            else if (ev->key == InputKeyUp) {
                int ty = m->cur_y;
                while(ty > 0) { ty--; if(get_elm_xy(m->cur_x, ty)) { m->cur_y = ty; break; } }
            }
            else if (ev->key == InputKeyDown) {
                int ty = m->cur_y;
                while(ty < 9) { ty++; if(get_elm_xy(m->cur_x, ty)) { m->cur_y = ty; break; } }
            }
        }, true); 
        return true; 
    }
    return false;
}

static uint32_t grid_b(void* ctx) { UNUSED(ctx); return VIEW_EXIT_PROMPT; }
static uint32_t to_grid(void* ctx) { UNUSED(ctx); return VIEW_GRID; }

static bool main_ev(void* ctx, uint32_t ev) {
    App* a = (App*)ctx;
    if (ev == EVENT_START_APP) { view_dispatcher_switch_to_view(a->disp, VIEW_GRID); return true; }
    if (ev == EVENT_FINAL_CLOSE) { view_dispatcher_stop(a->disp); return true; }
    
    // ОКНО СВОЙСТВ! 
    if (ev == EVENT_SHOW_DETAILS) {
        uint8_t x=0, y=0;
        with_view_model(a->v_grid, ViewModel* m, { x=m->cur_x; y=m->cur_y; }, false);
        const Element* e = get_elm_xy(x, y);
        if (e) { 
            snprintf(a->buf, sizeof(a->buf), 
                 "[ %s (%s) ] Z:%d\n"
                 "---------------\n"
                 "DISCOVERER:\n %s\n"
                 "CAS: %s\n"
                 "\n=== ATOMIC INFO ===\n"
                 "CAT: %s\n"
                 "BLK: %s | OX: %s\n"
                 "GRP: %d | PER: %d\n"
                 "CRYST: %s\n"
                 "\n=== SIZES (pm) ===\n"
                 "ATM: %s | COV: %s\n"
                 "ION: %s\n"
                 "\n=== PHYSICAL ===\n"
                 "MASS: %s\n"
                 "DENS: %s g/cm3\n"
                 "MELT: %s K\n"
                 "BOIL: %s K\n"
                 "MAGN: %s\n"
                 "\n=== QUANTUM ===\n"
                 "AFFIN: %s kJ/mol\n"
                 "EL.NG: %s\n"
                 "ION_E: %s kJ/mol\n"
                 "ORBIT: %s", 
                 e->name, e->symbol, e->z, e->disc, e->cas, e->cat, e->blk, e->ox, e->group, e->period, e->cryst,
                 e->r_a, e->r_c, e->r_i, e->mass, e->dens, e->melt, e->boil, e->mag, e->aff, e->pauling, e->ie, e->e_conf);
            text_box_set_text(a->v_details, a->buf);
            view_dispatcher_switch_to_view(a->disp, VIEW_DETAILS);
        } return true;
    }
    // ABOUT ОКНО
    if (ev == EVENT_SHOW_ABOUT) {
        snprintf(a->buf, sizeof(a->buf), 
                 "Flipper Elements v1.0\n"
                 "\n"
                 "Periodic Table of Elements\nfor Flipper Zero.\n"
                 "Author: Siarhei Besarab\naka steanlab.\n"
                 "\n"
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
                 "donorbox.org/donations-for-\nlab-66\n"
                 "\n"
                 "Thank You!");
        text_box_set_text(a->v_about, a->buf);
        view_dispatcher_switch_to_view(a->disp, VIEW_ABOUT);
        return true;
    } return false;
}

int32_t flipper_elements_app(void* p) {
    UNUSED(p);

    App* a = malloc(sizeof(App));
    memset(a, 0, sizeof(App)); 

    a->gui = furi_record_open(RECORD_GUI);
    a->disp = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(a->disp, a->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(a->disp, a);
    view_dispatcher_set_custom_event_callback(a->disp, main_ev); 

    a->t_splash = furi_timer_alloc(s_t_cb, FuriTimerTypeOnce, a);
    a->v_splash = view_alloc();
    view_set_context(a->v_splash, a); 
    view_set_draw_callback(a->v_splash, start_splash_draw);
    view_set_enter_callback(a->v_splash, s_en_cb);
    view_dispatcher_add_view(a->disp, VIEW_SPLASH, a->v_splash);

    a->v_grid = view_alloc();
    view_set_context(a->v_grid, a); 
    view_allocate_model(a->v_grid, ViewModelTypeLocking, sizeof(ViewModel));
    with_view_model(a->v_grid, ViewModel* m, { m->cur_x=0; m->cur_y=0; }, false); 
    view_set_draw_callback(a->v_grid, grid_draw);
    view_set_input_callback(a->v_grid, grid_input);
    view_set_previous_callback(a->v_grid, grid_b); 
    view_dispatcher_add_view(a->disp, VIEW_GRID, a->v_grid);

    a->v_details = text_box_alloc();
    text_box_set_font(a->v_details, TextBoxFontText);
    View* v_d = text_box_get_view(a->v_details);
    view_set_previous_callback(v_d, to_grid);
    view_dispatcher_add_view(a->disp, VIEW_DETAILS, v_d);

    a->v_about = text_box_alloc();
    text_box_set_font(a->v_about, TextBoxFontText);
    View* v_ab = text_box_get_view(a->v_about);
    view_set_previous_callback(v_ab, to_grid);
    view_dispatcher_add_view(a->disp, VIEW_ABOUT, v_ab);

    a->dialog = dialog_ex_alloc();
    dialog_ex_set_context(a->dialog, a);
    dialog_ex_set_header(a->dialog, "Exit Application?", 64, 18, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(a->dialog, "No");
    dialog_ex_set_right_button_text(a->dialog, "Yes");
    dialog_ex_set_result_callback(a->dialog, diag_res_cb);
    view_dispatcher_add_view(a->disp, VIEW_EXIT_PROMPT, dialog_ex_get_view(a->dialog));

    a->t_exit = furi_timer_alloc(e_t_cb, FuriTimerTypeOnce, a);
    a->v_exit_splash = view_alloc();
    view_set_context(a->v_exit_splash, a);
    view_set_draw_callback(a->v_exit_splash, exit_splash_draw);
    view_set_enter_callback(a->v_exit_splash, e_en_cb);
    view_dispatcher_add_view(a->disp, VIEW_EXIT_SPLASH, a->v_exit_splash);


    view_dispatcher_switch_to_view(a->disp, VIEW_SPLASH);
    view_dispatcher_run(a->disp);

    view_dispatcher_remove_view(a->disp, VIEW_SPLASH);
    view_dispatcher_remove_view(a->disp, VIEW_GRID);
    view_dispatcher_remove_view(a->disp, VIEW_DETAILS);
    view_dispatcher_remove_view(a->disp, VIEW_ABOUT);
    view_dispatcher_remove_view(a->disp, VIEW_EXIT_PROMPT);
    view_dispatcher_remove_view(a->disp, VIEW_EXIT_SPLASH);

    view_free(a->v_splash);
    view_free(a->v_grid);
    view_free(a->v_exit_splash);
    text_box_free(a->v_details);
    text_box_free(a->v_about);
    dialog_ex_free(a->dialog); 
    
    furi_timer_free(a->t_splash);
    furi_timer_free(a->t_exit);
    view_dispatcher_free(a->disp);
    furi_record_close(RECORD_GUI);
    free(a); 

    return 0;
}
