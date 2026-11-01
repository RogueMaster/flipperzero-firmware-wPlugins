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
	
	set hunter_list_1 to "0"
	set hunter_list_2 to "0"
	set hunter_list_3 to "0"
	set hunter_list_4 to "0"
	set hunter_list_5 to "0"
	set hunter_list_6 to "0"
	set hunter_list_7 to "0"
	set hunter_list_8 to "0"
	set hunter_list_9 to "0"
	set hunter_list_10 to "0"
	
	display dialog "You have chosen to be a hunter!
Do you want to go hunt or stay put?

1- Hunt; 2- Stay put
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set hunter_list_1 to text returned of result
	if hunter_list_1 is equal to "1" then
		display dialog "You have chosen to go hunt!
You have gone hunting.
You didn't find any food.
You return to your village.
Luckily, the farmer has enough crops to last your tribe a week.
Do you want to go hunt right away to make up lost time or do you choose to go eat?

1- Hunt; 2- Eat
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_2 to text returned of result
	end if
	if hunter_list_1 is equal to "2" then
		display dialog "You have chosen to stay put.
Unfortunately, your whole village parishes from hunger.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_2 is equal to "1" then
		display dialog "You have chosen to go hunt!
You go on a successful hunt that should last your tribe a month.
However, you are very tired and because you didn't eat, you have become very sick with malnutrition.
Unluckily for you, the tribe's medicine woman is very busy and has a lot of patients to take care of first.
Do you wish to wait for her or tough it out?

1- Wait for her; 2- Tough it out
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_3 to text returned of result
	end if
	if hunter_list_2 is equal to "2" then
		display dialog "You have chosen to eat.
You are fully fueled with energy.
Do you want to go hunting or play a game with the rest of your tribe?

1- Hunt; 2- Play a game with the rest of my tribe
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_4 to text returned of result
	end if
	if hunter_list_3 is equal to "1" then
		display dialog "You have chosen to wait for her.
Because you have chosen to wait for her patiently, she attends to you in a matter of a couple days.
You are better in a week.
Some white men have come to your tribe.
They're asking to trade you guns for some of your tribe's animal pelts.
Do you trade them pelts for guns?

1- Trade; 2- Not trade
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_5 to text returned of result
	end if
	if hunter_list_3 is equal to "2" then
		display dialog "You have chosen to tough it out.
Unfortunately, because of this, you die from malnutrition.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_4 is equal to "1" then
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
		set hunter_list_6 to text returned of result
	end if
	if hunter_list_4 is equal to "2" then
		display dialog "You have chosen to play a game with the rest of your tribe.
Everyone has a good time and you guys have an awesome feast.
There is a hunting challenge that your tribe is having.
The challenge is to go hunt The Mighty Lion!
Do you want to go hunt The Mighty Lion with your tribe to win the challenge?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_7 to text returned of result
	end if
	if hunter_list_5 is equal to "1" then
		display dialog "You have chosen to trade.
They trade you guns.
Do you want to go hunt The Mighty Lion?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_8 to text returned of result
	end if
	if hunter_list_5 is equal to "2" then
		display dialog "You have chosen not to trade.
Because of this they want to kill you.
Do you escape?

1- Find out!
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_9 to text returned of result
	end if
	if hunter_list_6 is equal to "1" then
		display dialog "You have chosen to trade.
Luckily, these aren't the criminals that are wanted.
A couple months later the white men settle here.
People keep on flooding into the white men's settlement.
Because of this your village becomes rich.
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_6 is equal to "2" then
		display dialog "You have chosen to decline the trade.
Unfortunately, these people have smallpox.
They transmit it to your tribe.
Because your tribe doesn't have any immunity against smallpox your whole tribe dies!
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_6 is equal to "3" then
		display dialog "You have chosen to report them to the authorities.
These guys turn out to be the wanted criminals after all.
Your tribe gains importance in the African community because you turned these criminals into the authorities.
Do you want to go hunt The Mighty Lion?

1- Yes; 2- No
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
		set hunter_list_10 to text returned of result
	end if
	if hunter_list_7 is equal to "1" then
		the_mighty_lion()
	end if
	if hunter_list_7 is equal to "2" then
		display dialog "You have chosen not to hunt The Mighty Lion.
Unfortunately, you die from malaria.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_8 is equal to "1" then
		the_mighty_lion()
	end if
	if hunter_list_8 is equal to "2" then
		display dialog "You have chosen not to hunt The Mighty Lion.
Unfortunately, a random lightning bolt from out of nowhere, probably from Zeus, kills you.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_9 is equal to "1" and rand is equal to 1 then
		display dialog "Unfortunately, you don't escape.
They kill you on the spot with a feather.
" & death_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_9 is equal to "1" and rand is equal to 2 then
		display dialog "You manage to escape.
You live a long and prosperous life!
" & victory_level with title "Volor Savanna"
		volor_savanna_original()
	end if
	if hunter_list_10 is equal to "1" then
		the_mighty_lion()
	end if
	if hunter_list_10 is equal to "2" then
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
	
end warrior

on crafter()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
end crafter

on farmer()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
end farmer

on medicine_person()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
end medicine_person

on the_mighty_lion()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	
end the_mighty_lion

ask_name()