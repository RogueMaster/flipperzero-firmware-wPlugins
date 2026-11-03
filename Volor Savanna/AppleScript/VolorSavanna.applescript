on ask_name()
	global death_level
	global victory_level
	global user_name
	
	display dialog "What is your name?" default answer "" with title "Volor Savanna"
	
	set user_name to text returned of result
	
	set death_level to user_name & ", YOU LOSE! THE END!"
	set victory_level to user_name & ", YOU WIN! THE END!"
	
	volor_savanna_original()
	
end ask_name

on volor_savanna_original()
	global death_level
	global victory_level
	global user_name
	
	display dialog "Welcome to Volor Savanna!
You are a member of an African Tribe!
Who do you want to be?

1- Hunter
2- Warrior
3- Crafter
4- Farmer
5- Medicine Person
e- exit

" default answer "" with title "Volor Savanna"
	
	set my_character to text returned of result
	
	if my_character is equal to "1" then
		hunter()
		
	else if my_character is equal to "2" then
		warrior()
		
	else if my_character is equal to "3" then
		crafter()
		
	else if my_character is equal to "4" then
		farmer()
		
	else if my_character is equal to "5" then
		medicine_person()
		
	else if my_character is equal to "e" then
		return
	end if
	
end volor_savanna_original

on hunter()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
	set hunter_1 to "0"
	set hunter_2 to "0"
	set hunter_3 to "0"
	set hunter_4 to "0"
	set hunter_5 to "0"
	set hunter_6 to "0"
	set hunter_7 to "0"
	set hunter_8 to "0"
	set hunter_9 to "0"
	set hunter_10 to "0"
	
	display dialog "You have chosen to be a hunter!
Do you want to go hunt or stay put?

1- Hunt; 2- Stay put
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set hunter_1 to text returned of result
	
	if hunter_1 is equal to "1" then
		display dialog "You have chosen to go hunt!
You have gone hunting.
You didn't find any food.
You return to your village.
Luckily, the farmer has enough crops to last your tribe a week.
Do you want to go hunt right away to make up lost time or do you choose to go eat?

1- Hunt; 2- Eat
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_2 to text returned of result
	end if
	
	if hunter_1 is equal to "2" then
		display dialog "You have chosen to stay put.
Unfortunately, your whole village parishes from hunger.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_2 is equal to "1" then
		display dialog "You have chosen to go hunt!
You go on a successful hunt that should last your tribe a month.
However, you are very tired and because you didn't eat, you have become very sick with malnutrition.
Unluckily for you, the tribe's medicine woman is very busy and has a lot of patients to take care of first.
Do you wish to wait for her or tough it out?

1- Wait for her; 2- Tough it out
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_3 to text returned of result
	end if
	
	if hunter_2 is equal to "2" then
		display dialog "You have chosen to eat.
You are fully fueled with energy.
Do you want to go hunting or play a game with the rest of your tribe?

1- Hunt; 2- Play a game with the rest of my tribe
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_4 to text returned of result
	end if
	
	if hunter_3 is equal to "1" then
		display dialog "You have chosen to wait for her.
Because you have chosen to wait for her patiently, she attends to you in a matter of a couple days.
You are better in a week.
Some white men have come to your tribe.
They're asking to trade you guns for some of your tribe's animal pelts.
Do you trade them pelts for guns?

1- Trade; 2- Not trade
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_5 to text returned of result
	end if
	
	if hunter_3 is equal to "2" then
		display dialog "You have chosen to tough it out.
Unfortunately, because of this, you die from malnutrition.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_4 is equal to "1" then
		display dialog "You have chosen to hunt.
You have found a bunch of zebras to eat.
Some white men are willing to trade your tribe rifles for some of your zebras.
Recently, there have been some highly wanted criminals that trade weapons and drugs on the black market.
The problem is, you don't know if that's them.
Do you trade?
What about decline the offer?
Or do you report them to the authorities?

1- Trade; 2- Decline the trade; 3- Report them to the authorities
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_6 to text returned of result
	end if
	
	if hunter_4 is equal to "2" then
		display dialog "You have chosen to play a game with the rest of your tribe.
Everyone has a good time and you guys have an awesome feast.
There is a hunting challenge that your tribe is having.
The challenge is to go hunt The Mighty Lion!
Do you want to go hunt The Mighty Lion with your tribe to win the challenge?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_7 to text returned of result
	end if
	
	if hunter_5 is equal to "1" then
		display dialog "You have chosen to trade.
They trade you guns.
Do you want to go hunt The Mighty Lion?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_8 to text returned of result
	end if
	
	if hunter_5 is equal to "2" then
		display dialog "You have chosen not to trade.
Because of this they want to kill you.
Do you escape?

1- Find out!
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_9 to text returned of result
	end if
	
	if hunter_6 is equal to "1" then
		display dialog "You have chosen to trade.
Luckily, these aren't the criminals that are wanted.
A couple months later the white men settle here.
People keep on flooding into the white men's settlement.
Because of this your village becomes rich.
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_6 is equal to "2" then
		display dialog "You have chosen to decline the trade.
Unfortunately, these people have smallpox.
They transmit it to your tribe.
Because your tribe doesn't have any immunity against smallpox your whole tribe dies!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_6 is equal to "3" then
		display dialog "You have chosen to report them to the authorities.
These guys turn out to be the wanted criminals after all.
Your tribe gains importance in the African community because you turned these criminals into the authorities.
Do you want to go hunt The Mighty Lion?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_10 to text returned of result
	end if
	
	if hunter_7 is equal to "1" then
		the_mighty_lion()
	end if
	
	if hunter_7 is equal to "2" then
		display dialog "You have chosen not to hunt The Mighty Lion.
Unfortunately, you die from malaria.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_8 is equal to "1" then
		the_mighty_lion()
	end if
	
	if hunter_8 is equal to "2" then
		display dialog "You have chosen not to hunt The Mighty Lion.
Unfortunately, a random lightning bolt from out of nowhere, probably from Zeus, kills you.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_9 is equal to "1" and rand is equal to 1 then
		display dialog "Unfortunately, you don't escape.
They kill you on the spot with a feather.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_9 is equal to "1" and rand is equal to 2 then
		display dialog "You manage to escape.
You live a long and prosperous life!
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if hunter_10 is equal to "1" then
		the_mighty_lion()
	end if
	
	if hunter_10 is equal to "2" then
		display dialog "You have chosen not to hunt The Mighty Lion.
Unfortunately, a Chandra Planeswalker burns you to death!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	
	
end hunter

on warrior()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
	set warrior_1 to "0"
	set warrior_2 to "0"
	set warrior_3 to "0"
	set warrior_4 to "0"
	set warrior_5 to "0"
	
	display dialog "You have chosen to be a warrior!
Do you want to go hunt The Mighty Lion or stay put?

1- Hunt The Mighty Lion; 2- Stay put
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set warrior_1 to text returned of result
	
	if warrior_1 is equal to "1" then
		the_mighty_lion()
	end if
	
	if warrior_1 is equal to "2" then
		display dialog "You have chosen to stay put.
Without warning a neighboring tribe attacks.
Do you choose to get your bow and arrows in your hut, or do you choose to fight them off with your knife on hand?

1- Go get my bow and arrows; 2- Use my knife
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set warrior_2 to text returned of result
	end if
	
	if warrior_2 is equal to "1" then
		display dialog "You have chosen to go get your bow and arrows.
Unfortunately, the enemy beats you to it and shoots you on the spot.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if warrior_2 is equal to "2" then
		display dialog "You have chosen to use your knife.
You are very skilled with a knife.
Eventually the enemy tribe retreats.
The chief has told you to go take a weekend off at the Big Pond.
You go to the Big Pond and enjoy it very much.
You see a crying baby by the side of the Big Pond.
Do you pick up the baby and bring him home or just ignore him?

1- Pick him up and bring him home; 2- Just ignore him
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set warrior_3 to text returned of result
	end if
	
	if warrior_3 is equal to "1" then
		display dialog "You have chosen to pick up the baby and bring him home.
The baby turns out to be an orphan.
His mother was killed by the enemy tribe and the baby was left by the Big Pond.
You raise him as your own.
He becomes a great warrior and you name him Bobert.
You need to go hunt down the enemy that attacked your tribe years ago.
Do you take your son with you?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set warrior_4 to text returned of result
	end if
	
	if warrior_3 is equal to "2" then
		display dialog "You have chosen to just ignore him.
Do you choose to go after the enemy tribe with the few warriors you have left?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set warrior_5 to text returned of result
	end if
	
	if warrior_4 is equal to "1" then
		display dialog "You have chosen to take your son with you.
Your hunting party would have died if you didn't have your son.
Since your son is a great warrior the enemy tribe which you now know as Yosemite plans on never attacking your tribe again!
You return home and find out your son has a brand-new kid.
Bobert, your son, user_names him Mighty " & user_name & " Jr.
You finally have a grandson.
You die at a good old age.
You pass away happily in your sleep.
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if warrior_4 is equal to "2" then
		display dialog "You have chosen not to take your son with you.
Because you didn't take your son, the enemy tribe, Yosemite, kills your whole hunting party.
You guys are no match for them!
Unfortunately, while running away from Yosemite you trip over a rock and die!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if warrior_5 is equal to "1" then
		display dialog "You go after the enemy tribe with the few warriors you have left.
Unfortunately, while on your way The Mighty Pac-Man gobbles you up.
You guys are no match for The Mighty Pac-Man!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if warrior_5 is equal to "2" then
		display dialog "You have chosen not to go after the enemy tribe.
Unfortunately, your village gets swept away by a flash flood!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
end warrior

on crafter()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
	set crafter_1 to "0"
	set crafter_2 to "0"
	set crafter_3 to "0"
	set crafter_4 to "0"
	set crafter_5 to "0"
	set crafter_6 to "0"
	
	display dialog "You have chosen to be a crafter!
You need to craft some arrows.
How do you want to craft them?
1- Put red dye on the feathers; 2- Just make them
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set crafter_1 to text returned of result
	
	if crafter_1 is equal to "1" then
		display dialog "You have chosen to put red dye on the feathers!
Because you put red dye on the feathers none were lost so the hunters were able to bring home more food!
Because of this you have one month's worth of food for the winter!
You have been asked to craft some spearheads.
How do you want to craft them?
1- Put poison on the spearheads; 2- Just make them
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set crafter_2 to text returned of result
	end if
	
	if crafter_1 is equal to "2" then
		display dialog "You have chosen to just make the arrows.
Unfortunately, because of this the hunters lost some arrows and didn't bring home much food.
Even worse than that, Master Chief comes into your tribe and pwns all of you!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if crafter_2 is equal to "1" then
		display dialog "You have chosen to put poison on the spear heads.
The animals die easier and your tribe gets more food for the winter.
To be exact, one more month of food.
Some white men come to your tribe.
They're paying a fortune for you to craft them some of your best acacia furniture.
Do you want to craft some expensive acacia furniture for them, or do you not want to craft them the furniture their asking for?
1- Craft them furniture; 2- Not craft them furniture
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set crafter_3 to text returned of result
	end if
	
	if crafter_2 is equal to "2" then
		display dialog "You have chosen to just make the spear heads.
Because you didn't put poison on the spear heads the hunters only brought back three gazelles.
Unfortunately, the gazelles get mold on them.
Even worse, you eat the moldy gazelles and die from food poisoning.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if crafter_3 is equal to "1" then
		display dialog "You have chosen to craft them furniture.
The people you give the furniture to like your work.
They pay you in gold.
You can use this to buy a milling machine from a city nearby.
Do you buy a milling machine?
1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set crafter_4 to text returned of result
	end if
	
	if crafter_3 is equal to "2" then
		display dialog "You have chosen not to craft them furniture.
Because of this they want to kill you.
Do you escape?
1- Find out!
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set crafter_5 to text returned of result
	end if
	
	if crafter_4 is equal to "1" then
		display dialog "You have chosen to buy a milling machine.
Some business men have come to your tribe and are asking you to build them some wooden toys with your milling machine.
Do you want to build them wooden toys with your milling machine?
1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set crafter_6 to text returned of result
	end if
	
	if crafter_4 is equal to "2" then
		display dialog "You have chosen not to buy a milling machine.
Unfortunately, your tribe dies from a famine.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if crafter_5 is equal to "1" and rand is equal to 1 then
		display dialog "You escape.
Unfortunately, an asteroid hits the earth and destroys the whole planet.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if crafter_5 is equal to "1" and rand is equal to 2 then
		display dialog "You don't escape.
They kill you on the spot.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if crafter_6 is equal to "1" then
		display dialog "You have chosen to use your milling machine to make them wooden toys.
You have had a successful life!
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if crafter_6 is equal to "2" then
		display dialog "You have chosen not to make them wooden toys with your milling machine.
Unfortunately, your milling machine catches on fire and burns down your whole house.
What's even more unfortunate is that you get caught on fire and burn to death.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
end crafter

on farmer()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
	set farmer_1 to "0"
	set farmer_2 to "0"
	set farmer_3 to "0"
	set farmer_4 to "0"
	set farmer_5 to "0"
	set farmer_6 to "0"
	
	display dialog "You have chosen to be a farmer!
Do you want to go farm or sleep in?
1- Farm; 2- Sleep in
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set farmer_1 to text returned of result
	
	if farmer_1 is equal to "1" then
		display dialog "You have chosen to farm!
You have had a successful season!
Your village has a feast.
Do you want to store the rest of the food for winter or distribute the food to the whole village?
1- Store the food for winter; 2- Distribute the food to the whole village
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set farmer_2 to text returned of result
	end if
	
	if farmer_1 is equal to "2" then
		display dialog "You have chosen to sleep in.
Unfortunately, a prairie fire destroys your crops and your whole village.
Everyone has escaped and survived.
Everyone except for one person.
You.
You have died in the fire.
You were burnt into ashes in your sleep.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if farmer_2 is equal to "1" then
		display dialog "You have chosen to store the food for winter.
Because you have chosen to store the food for winter your tribe doesn't go hungry.
Last year you grew wheat.
Do you want to grow wheat again this year or farm corn for the first time in your life?
1- Grow wheat again; 2- Farm corn
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set farmer_3 to text returned of result
	end if
	
	if farmer_2 is equal to "2" then
		display dialog "You have chosen to distribute the food to the whole village.
Everyone has enough food for the whole winter.
Unfortunately, the food rots in a winter storm.
Your village dies of starvation.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if farmer_3 is equal to "1" then
		display dialog "You have chosen to grow wheat again for the second year in a row.
Unfortunately, there are no nutrients in the soil so all your wheat dies.
Your village starves.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if farmer_3 is equal to "2" then
		display dialog "You have chosen to farm corn for the first time in your life.
Luckily for you, someone else in your village knows how to grow corn.
Because of this you grow a lot of corn and your village has a feast.
Do you want to store the rest of the food for the winter or distribute the rest of the food to your whole village?
1- Store the food for winter; 2- Distribute the food to your whole village
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set farmer_4 to text returned of result
	end if
	
	if farmer_4 is equal to "1" then
		display dialog "You have chosen to store the food for winter.
The corn lasts your tribe the whole winter.
Your tribe is thriving!
You find out that you have extra corn.
It would be nice to get some oxen to help you plow the field.
Do you sell your extra corn for oxen?
1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set farmer_5 to text returned of result
	end if
	
	if farmer_4 is equal to "2" then
		display dialog "You have chosen to distribute the food to your whole village.
Unfortunately, because you have chosen to do this, your food rots.
Your whole village starves.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if farmer_5 is equal to "1" then
		display dialog "You have chosen to sell your extra corn for oxen.
You choose to grow cotton.
At the end of the season you have surplus cotton.
Do you want to sell your surplus cotton?
1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set farmer_6 to text returned of result
	end if
	
	if farmer_5 is equal to "2" then
		display dialog "You have chosen not to sell your extra corn for oxen.
Unfortunately, Bob the Blobfish plops out from a nearby lake and you die because your eyes bleed to death.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if farmer_6 is equal to "1" then
		display dialog "You have chosen to sell your surplus cotton.
You are now rich!
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if farmer_6 is equal to "2" then
		display dialog "You have chosen not to sell your surplus cotton.
Unfortunately, a wizard says some magic words that make you vanish.
You are never seen again.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
end farmer

on medicine_person()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
	set medicine_person_1 to "0"
	set medicine_person_2 to "0"
	set medicine_person_3 to "0"
	set medicine_person_4 to "0"
	set medicine_person_5 to "0"
	set medicine_person_6 to "0"
	set medicine_person_7 to "0"
	set medicine_person_8 to "0"
	set medicine_person_9 to "0"
	set medicine_person_10 to "0"
	set medicine_person_11 to "0"
	set medicine_person_12 to "0"
	
	
	display dialog "You have chosen to be a medicine person!
A man is very sick with a fever.
What do you want to do?
1- Not do anything; 2- Wash him
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set medicine_person_1 to text returned of result
	
	if medicine_person_1 is equal to "1" then
		display dialog "You have chosen not to do anything.
Unfortunately, the man dies.
The man's family is very angry at you.
You are tried for murder.
You have been found guilty.
So you are put to death.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_1 is equal to "2" then
		display dialog "You have chosen to wash him.
The man recovers from his fever.
You are a hero in your village.
The chief rewards you with a big meal.
The chief offers you a week off.
What do you do?
1- Take a week off; 2- Tend to more sick people
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_2 to text returned of result
	end if
	
	if medicine_person_2 is equal to "1" then
		display dialog "You have chosen to take a week off.
You come back and someone is very sick.
You are low on herbs.
Do you go gather more herbs for your remedies or heal the patient with what you have?
1- Go gather more herbs; 2- Heal the patient
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_3 to text returned of result
	end if
	
	if medicine_person_2 is equal to "2" then
		display dialog "You have chosen to tend to more sick people.
You are very good at your craft.
You heal most of the people.
However, you are low on herbs.
Do you go gather some more herbs or tend to a couple more patients?
1- Go gather more herbs; 2- Go tend to a couple more patients
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_4 to text returned of result
	end if
	
	if medicine_person_3 is equal to "1" then
		display dialog "You have chosen to go gather more herbs.
Because of this the patient dies!
You are tried for murder.
Are you found guilty?
1- Find out!
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_5 to text returned of result
	end if
	
	if medicine_person_3 is equal to "2" then
		display dialog "You have chosen to heal the patient.
Luckily, you have enough herbs to heal the patient.
You need to gather more herbs.
You go gather some more herbs.
While gathering more herbs you find a signal that says: \"S.O.S. >\"
What do you do?
1- Go investigate the distress signal; 2- Ignore it; 3- Go back to my tribe and tell someone about it
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_6 to text returned of result
	end if
	
	if medicine_person_4 is equal to "1" then
		display dialog "You have chosen to gather more herbs.
You go gather more herbs.
Luckily, none of the patients die and you heal all of them.
While gathering more herbs you find a signal that says: \"S.O.S. >\"
1- Go investigate the distress signal; 2- Ignore it; 3- Tell someone about it
What do you do?
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_7 to text returned of result
	end if
	
	if medicine_person_4 is equal to "2" then
		display dialog "You have chosen to go tend to a couple more patients.
You run out of herbs so you go gather more herbs.
Unfortunately, while gathering herbs, all your patients die!
You are tried for murder.
You are found guilty.
How do you want to be executed?
1- Arrows; 2- Sword; 3- Flying lawnmower; 4- Musket; 5- Bob the Blobfish
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_8 to text returned of result
	end if
	
	if medicine_person_5 is equal to "1" and rand is equal to 1 then
		display dialog "Unfortunately, you have been found guilty.
How do you want to be executed?
1- Arrows; 2- Sword; 3- Stoned; 4- Knife; 5- Gun
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_9 to text returned of result
	end if
	
	if medicine_person_5 is equal to "1" and rand is equal to 2 then
		display dialog "You are not found guilty.
Some white men come to your tribe.
They're asking you if they can settle near you.
Do you want them to settle near you?
1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_10 to text returned of result
	end if
	
	if medicine_person_6 is equal to "1" or medicine_person_7 then
		display dialog "You have chosen to go investigate the distress signal.
Unfortunately, you get eaten by a pack of dingos.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_6 is equal to "2" or medicine_person_7 then
		display dialog "You have chosen to ignore it.
Unfortunately, on your way back you get eaten by a cheetah.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_6 is equal to "3" or medicine_person_7 then
		display dialog "You have chosen to tell someone about it.
Apparently, some members in your tribe say a mother was kidnapped and that there is a $10,000 reward for the capture of the kidnapper!
Do you want to go hunt the kidnapper?
1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_11 to text returned of result
	end if
	
	if medicine_person_8 is equal to "1" then
		display dialog "You have chosen to die from arrows.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_8 is equal to "2" then
		display dialog "You have chosen to die from a sword.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_8 is equal to "3" then
		display dialog "You have chosen to die from a flying lawnmower.
Seriously though, how do you manage to die from a flying lawnmower?!
I guess someone strapped jet engines and/or rockets to a lawnmower and it became a flying lawnmower!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_8 is equal to "4" then
		display dialog "You have chosen to die from a musket.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_8 is equal to "5" then
		display dialog "You have chosen to die from Bob the Blobfish.
Bob the Blobfish rockets out of his toilet and makes your eyes bleed to death!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_9 is equal to "1" then
		display dialog "You die from arrows.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_9 is equal to "2" then
		display dialog "You get stabbed to death by a sword.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_9 is equal to "3" then
		display dialog "You get stoned to death.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_9 is equal to "4" then
		display dialog "You get stabbed to death by a knife.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_9 is equal to "5" then
		display dialog "You are shot to death with a bebe gun.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_10 is equal to "1" then
		display dialog "You have chosen to let them settle here.
They build a hospital.
Their settlement grows.
Your village thrives because of the new settlement.
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_10 is equal to "2" then
		display dialog "You do not let them settle here.
Unfortunately, Cooper the Dog hears a doorbell on TV.
He goes to bark at the door.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_11 is equal to "1" then
		display dialog "You hunt the kidnapper.
Do you capture him?
1- Find out!
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set medicine_person_12 to text returned of result
	end if
	
	if medicine_person_11 is equal to "2" then
		display dialog "You don't hunt the kidnapper.
Unfortunately, Zeus comes out from the middle of nowhere and shocks you to death.
How shocking!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_12 is equal to "1" and rand is equal to 1 then
		display dialog "You don't capture the kidnapper.
Unfortunately, the kidnapper puts you guys in a trap and kills you.
You die.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if medicine_person_12 is equal to "1" and rand is equal to 2 then
		display dialog "You capture the kidnapper.
You receive $10k from the local authorities.
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
end medicine_person

on the_mighty_lion()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
	set the_mighty_lion_1 to "0"
	set the_mighty_lion_2 to "0"
	set the_mighty_lion_3 to "0"
	set the_mighty_lion_4 to "0"
	set the_mighty_lion_5 to "0"
	set the_mighty_lion_6 to "0"
	
	display dialog "You have chosen to hunt The Mighty Lion.
You have been on your journey for about a week now.
The trail splits off into three sections.
Do you choose to go through the canyon?
How about the prairie?
Or what about continuing on through the forest?

1- Canyon; 2- Prairie; 3- Forest
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set the_mighty_lion_1 to text returned of result
	
	if the_mighty_lion_1 is equal to "1" then
		display dialog "You have chosen to go through the canyon!
The canyon is the quickest route to The Mighty Lion.
Unfortunately, you're almost out of the canyon when a flash flood occurs.
You get wiped away instantly and you drown to death.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_1 is equal to "2" then
		display dialog "You have chosen to go through the prairie!
Which way do you travel?

1- Left; 2- Right
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_2 to text returned of result
	end if
	
	if the_mighty_lion_1 is equal to "3" then
		display dialog "You have chosen to go through the forest!
You see a river.
Do you drink from it?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_3 to text returned of result
	end if
	
	if the_mighty_lion_2 is equal to "1" then
		display dialog "You have chosen to go left.
Unfortunately, there are hyenas in front of you after only about an hour of walking.
They see you and kill you.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_2 is equal to "2" then
		display dialog "You have chosen to go right.
There are zebras in front of you after only about an hour of walking.
Do you kill them for food?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_4 to text returned of result
	end if
	
	if the_mighty_lion_3 is equal to "1" then
		the_mighty_lion_terrain()
	end if
	
	if the_mighty_lion_3 is equal to "2" then
		display dialog "You have chosen not to drink from the river.
Your fellow warriors have.
The river doesn't make them sick because the river comes from a far off glacier.
Unfortunately, you cross the river and find no more water sources.
You die from dehydration.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_4 is equal to "1" then
		display dialog "You have chosen to kill the zebras.
Because you have chosen to do this you don't go hungry.
Night falls.
Do you sleep on the ground or in a tree?

1- Ground; 2- Tree
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_5 to text returned of result
	end if
	
	if the_mighty_lion_4 is equal to "2" then
		display dialog "You have chosen not to kill the zebras.
Unfortunately, because of this, you starve.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_5 is equal to "1" then
		display dialog "You have chosen to sleep on the ground.
Unfortunately, a dingo comes up and gobbles you up.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_5 is equal to "2" then
		display dialog "You have chosen to sleep in a tree.
No animals eat you.
You see a river.
Do you drink from it?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_6 to text returned of result
	end if
	
	if the_mighty_lion_6 is equal to "1" then
		the_mighty_lion_terrain()
	end if
	
	if the_mighty_lion_6 is equal to "2" then
		display dialog "You have chosen not to drink from the river.
Your fellow warriors have.
The river doesn't make them sick because the river comes from a far off glacier.
Unfortunately, you cross the river and find no more water sources.
You die from dehydration.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
end the_mighty_lion

on the_mighty_lion_terrain()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
	set the_mighty_lion_terrain_1 to "0"
	set the_mighty_lion_terrain_2 to "0"
	set the_mighty_lion_terrain_3 to "0"
	set the_mighty_lion_terrain_4 to "0"
	
	display dialog "You have chosen to drink from the river.
Luckily, the water doesn't have any diseases or viruses.
You are full.
You need to cross the river.
However, the river is very deep.
You could make a temporary bridge but that would kill vital time to hunt The Mighty Lion.
The fastest way to cross the river is to make a raft.
What do you do?

1- Make a bridge; 2- Build a raft
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set the_mighty_lion_terrain_1 to text returned of result
	
	if the_mighty_lion_terrain_1 is equal to "1" then
		display dialog "You have chosen to make a bridge.
Your patience pays off and you get across the river in no time!
The path splits in two.1
Do you travel through the hills or the extreme hills?

1- Hills; 2- Extreme hills
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_terrain_2 to text returned of result
	end if
	
	if the_mighty_lion_terrain_1 is equal to "2" then
		display dialog "You have chosen to build a raft.
Unfortunately, the river brings you to the rapids before you are able to reach the other side.
The rapids are heading towards a waterfall, so you panic.
You throw your paddle into the river.
You fall down the waterfall and hit the rocks at the bottom.
Your head splits open and you die!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_terrain_2 is equal to "1" then
		display dialog "You have chosen to go through the hills.
You travel through the hills and see The Mighty Lion!
What weapon do you want to use?

1- Knife; 2- Bow and arrows; 3- Spear; 4- Gun
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_terrain_3 to text returned of result
	end if
	
	if the_mighty_lion_terrain_2 is equal to "2" then
		display dialog "You have chosen to go through the extreme hills.
You freeze to death.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_terrain_3 is equal to "1" then
		display dialog "You have chosen to use your trusty knife to kill The Mighty Lion!
So, " & user_name & " do you kill The Mighty Lion?

1- Find out!
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_terrain_4 to text returned of result
	end if
	
	if the_mighty_lion_terrain_3 is equal to "2" then
		display dialog "You have chosen to use your mighty bow and arrows to kill The Mighty Lion!
So, " & user_name & " do you kill The Mighty Lion?

1- Find out!
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_terrain_4 to text returned of result
	end if
	
	if the_mighty_lion_terrain_3 is equal to "3" then
		display dialog "You have chosen to use your powerful spear to kill The Mighty Lion.
So, " & user_name & " do you kill The Mighty Lion?

1- Find out!
" default answer "" with title "Volor Savanna"
		set the_mighty_lion_terrain_4 to text returned of result
	end if
	
	if the_mighty_lion_terrain_3 is equal to "4" then
		display dialog "You have chosen to use your overpowered gun to kill The Mighty Lion.
Unfortunately, your gun explodes in your face.
You die.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_terrain_4 is equal to "1" and rand is equal to 1 then
		display dialog "You do not kill The Mighty Lion.
Unfortunately, The Mighty Lion eats you!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
	if the_mighty_lion_terrain_4 is equal to "1" and rand is equal to 2 then
		display dialog "You kill The Mighty Lion!
Your tribe celebrates!
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	
end the_mighty_lion_terrain

ask_name()
