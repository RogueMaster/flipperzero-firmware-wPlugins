# App Data
## Decision Tree for Identifying European Tree Species
We started of with a prompt to deepseek.com:
> Generate a YAML-artifact for a decision tree to identify the following tree species. <br>
> Include English and German name (and Latin offical name as well). <br>
> [list of names]
This was not too successful. 

## List different species
We also tried
> Create a hierarchical markdown list of Central European tree species with German names in parentheses.<br>
> Use 🚨 for endangered species, 🌿 for abundant species, 🍎 for edible fruits<br>
> Group by coniferous/deciduous and botanical families. <br>
> Include these species: [list of names]
The result looks promissing, but I still want to double-check with literature.

* Coniferous Trees (Nadelbäume)
  * Spruce (Fichten)
    * Norway Spruce (Gemeine Fichte / Rotfichte) 🌿
  * Pine (Kiefern)
    * Scots Pine (Waldkiefer / Gemeine Kiefer) 🌿
    * Mountain Pine (Bergkiefer / Latschenkiefer)
    * Arolla Pine (Zirbelkiefer / Arve)
  * Fir (Tannen)
    * European Silver Fir (Weißtanne)
  * Larch (Lärchen)
    * European Larch (Europäische Lärche)
  * Other Conifers
    * Yew (Eibe) 🚨 🍎 (Aril is edible, but seeds are highly toxic)
    * Douglas Fir (Gewöhnliche Douglasie)
* Deciduous Trees (Laubbäume)
  * Beech Family (Buchengewächse)
    * European Beech (Rotbuche) 🌿
    * English / Pedunculate Oak (Stieleiche) 🌿
    * Sessile Oak (Traubeneiche) 🌿
    * Sweet Chestnut (Esskastanie / Edelkastanie) 🍎
    * European Hornbeam (Hainbuche / Weißbuche) 🌿
  * Maple Family (Ahorngewächse)
    * Norway Maple (Spitzahorn)
    * Sycamore Maple (Bergahorn)
    * Field Maple (Feldahorn) 🌿
  * Olive Family (Ölbaumgewächse)
    * Ash (Gemeine Esche) 🚨 (due to Ash Dieback)
    * Black Locust (Gewöhnliche Robinie / Scheinakazie)
  * Rose Family (Rosengewächse)
    * Wild Cherry (Vogelkirsche) 🍎
    * Rowan / Mountain Ash (Vogelbeere / Eberesche) 🍎 (berries are edible when processed)
    * Common Whitebeam (Gemeine Mehlbeere)
    * Common Hawthorn (Eingriffeliger Weißdorn)
    * Blackthorn (Schlehe) 🍎 (sloes are edible after frost)
  * Birch Family (Birkengewächse)
    * Silver Birch (Sandbirke / Hängebirke) 🌿
    * Black Alder (Schwarzerle)
  * Linden Family (Lindengewächse)
    * Small-leaved Lime (Winterlinde)
    * Large-leaved Lime (Sommerlinde)
  * Other Deciduous Trees
    * Hop Hornbeam (Gemeine Hopfenbuche)
    * Wych Elm (Bergulme) 🚨 (due to Dutch Elm Disease)
    * Aspen (Zitterpappel / Espe)
    * Common Hazel (Gemeine Hasel) 🌿 🍎
