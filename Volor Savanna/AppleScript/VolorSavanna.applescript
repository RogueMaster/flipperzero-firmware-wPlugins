on user_name()
	global death_level
	global victory_level
	global user_name
	
	display dialog "What is your name?" default answer "" with title "Volor Savanna"
	
	set user_name to text returned of result
	
	set death_level to user_name & ", YOU LOSE! THE END!"
	set victory_level to user_name & ", YOU WIN! THE END!"
	
	volor_savanna_original()
	
end user_name

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
	
	set choice to text returned of result
	
	if choice is equal to "1" then
		hunter()
		
	else if choice is equal to "2" then
		warrior()
		
	else if choice is equal to "3" then
		crafter()
		
	else if choice is equal to "4" then
		farmer()
		
	else if choice is equal to "5" then
		medicine_person()
		
	else if choice is equal to "e" then
		return
	end if
	
end volor_savanna_original

on hunter()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	log rand
	
	display dialog "You have chosen to be a hunter!
Do you want to go hunt or stay put?

1- Hunt; 2- Stay put
Make your choice " & user_name & ":
" default answer "" with title "Volor Savanna"
	set hunter_list_1 to text returned of result
	
end hunter

on warrior()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	log rand
	
end warrior

on crafter()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	log rand
	
end crafter

on farmer()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	log rand
	
end farmer

on medicine_person()
	global death_level
	global victory_level
	global user_name
	
	set rand to (random number from 1 to 2)
	log rand
	
end medicine_person

user_name()