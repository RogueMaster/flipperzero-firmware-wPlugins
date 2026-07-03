Cocktail Book is a simple Cocktail Guide for Flipper Zeron
It contains a couple classic Cocktails, sorted into Categorys and displayed wit a simple overview, detailed ingredients and preparation steps.
The App features a couple built in Cocktails as a fallback, but the Cocktails are mainly stored in a .txt file wich can be used to add your own Cocktails.

This Feature works thusly:
    1.Locate the recipes.txt file under etx/apps_data/cocktail_book/
    2. Add Cocktails based on this format:
  ```
    [Cocktail] //Cocktails start with [Cocktail]
    name=Negroni //The Name for your creation
    category=Classics //This will sort the Cocktail into the respective Category, currently these are available:Classic,Sour,Spritz,TikiDrinks,AlcoholFree,Digestive,Highball.
    base=Gin //Defines the Base Spirit
    glass=Tumbler //Defines the Glass, currently available: Martini, Margarita,Coupe,Tumbler,Wine,Highball.
    ice=Block // Defines which ice tu use, Available. crushed,cubes, block
    ingredients=30 ml Gin|30 ml Campari|30 ml Red Vermouth //Define your ingredients, use | for a new line.
    method=Stir all ingredients on ice.|Strain into Tumbler on ice. //Define preperation Method, use | for a new line.
    garnish=Orange zest //Define Garnish.
    note=Bitter, strong, classic. //Define Taste notes.
  ```
More Cocktails will be added in the Future.
Enjoy Mixing & Cheers.
    
