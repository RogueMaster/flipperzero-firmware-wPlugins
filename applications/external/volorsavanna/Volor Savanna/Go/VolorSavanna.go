package main

import (
	"fmt"
	"math/rand"
)

func user_name() {
	var name string
	fmt.Print("\033[H\033[2J")
	fmt.Println("What is your name?")
	fmt.Scan(&name)
	volor_savanna_original(name)
	//death_level := name + ", YOU LOSE!\nTHE END!"
	//victory_level := name + ", YOU WIN!\nTHE END!"
}

func volor_savanna_original(name string) {
	var character string
	fmt.Print("\033[H\033[2J")
	fmt.Println("Welcome to Volor Savanna!\nYou are a member of an African Tribe!\nWho do you want to be?\n\n1- Hunter\n2- Warrior\n3- Crafter\n4- Farmer\n5- Medicine Person\ne- exit")
	fmt.Scan(&character)

	if character == "1" {
		hunter(name)
	}
}

func hunter(name string) {
	rand := rand.Intn(2) + 1

	death_level := name + ", YOU LOSE!\nTHE END!"
	victory_level := name + ", YOU WIN!\nTHE END!"

	var pause string
	var hunter_1 string
	var hunter_2 string
	var hunter_3 string
	var hunter_4 string
	var hunter_5 string
	var hunter_6 string
	var hunter_7 string
	var hunter_8 string
	var hunter_9 string
	var hunter_10 string

	fmt.Print("\033[H\033[2J")
	fmt.Println("You have chosen to be a hunter!\nDo you want to go hunt or stay put?\n\n1- Hunt; 2- Stay put\nMake your choice " + name + ":")
	fmt.Scan(&hunter_1)

	if hunter_1 == "1" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to go hunt!\nYou have gone hunting.\nYou didn't find any food.\nYou return to your village.\nLuckily, the farmer has enough crops to last your tribe a week.\nDo you want to go hunt right away to make up lost time or do you choose to go eat?\n\n1- Hunt; 2- Eat\nMake your choice " + name + ":")
		fmt.Scan(&hunter_2)
	}

	if hunter_1 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to stay put.\nUnfortunately, your whole village parishes from hunger.\n" + death_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}

	if hunter_2 == "1" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to go hunt!\nYou go on a successful hunt that should last your tribe a month.\nHowever, you are very tired and because you didn't eat, you have become very sick with malnutrition.\nUnluckily for you, the tribes medicine women is very busy and has a lot of patients to take care of first.\nDo you wish to wait for her or tough it out?\n\n1- Wait for her; 2- Tough it out\nMake your choice " + name + ":")
		fmt.Scan(&hunter_3)
	}

	if hunter_2 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to eat.\nYou are fully fueled with energy.\nDo you want to go hunting or play a game with the rest of your tribe?\n\n1- Hunt; 2- Play a game with the rest of my tribe\nMake your choice " + name + ":")
		fmt.Scan(&hunter_4)
	}

	if hunter_3 == "1" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to wait for her.\nBecause You have chosen to wait for her patiently, she attends to you in a matter of a couple days.\nYou are better in a week.\nSome white men have come to your tribe.\nThey're asking to trade you guns for some of your tribe's animal pelts.\nDo you trade them pelts for guns?\n\n1- Trade; 2- Not trade\nMake your choice " + name + ":")
		fmt.Scan(&hunter_5)
	}

	if hunter_3 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to tough it out.\nUnfortunately, because of this, you die from malnutrition.\n" + death_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}

	if hunter_4 == "1" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to hunt.\nYou have found a bunch of zebras to eat.\nSome white men are willing to trade your tribe rifles for some of your zebras.\nRecently, there has been some highly wanted criminals that trade weapons and drugs on the black market.\nThe problem is, you don't know if that's them.\nDo you trade?\nWhat about decline the offer?\nOr do you report them to the authorities?\n\n1- Trade; 2- Decline the trade; 3- Report them to the authorities\nMake your choice " + name + ":")
		fmt.Scan(&hunter_6)
	}

	if hunter_4 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to play a game with the rest of your tribe.\nEveryone has a good time and you guys have an awesome feast.\nThere is a hunting challenge that your tribe is having.\nThe challenge is to go hunt The Mighty Lion!\nDo you want to go hunt The Mighty Lion with your tribe to win the challenge?\n\n1- Yes; 2- No\nMake your choice " + name + ":")
		fmt.Scan(&hunter_7)
	}

	if hunter_5 == "1" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to trade.\nThey trade you guns.\nDo you want to go hunt The Mighty Lion?\n\n1- Yes; 2- No\nMake your choice " + name + ":")
		fmt.Scan(&hunter_8)
	}

	if hunter_5 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen not to trade.\nBecause of this they want to kill you.\nDo you escape?\n\n1- Find out!\nMake your choice " + name + ":")
		fmt.Scan(&hunter_9)
	}

	if hunter_6 == "1" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to trade.\nLuckily, these aren't the criminals that are wanted.\nA couple months later the white men settle here.\nPeople keep on flooding into the white men's settlement.\nBecause of this your village becomes rich.\n" + victory_level)
		volor_savanna_original(name)
	}

	if hunter_6 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to decline the trade.\nUnfortunately, these people have smallpox.\nThey transmit it to your tribe.\nBecause you tribe doesn't have any immunity against smallpox your whole tribe dies!\n" + death_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}

	if hunter_6 == "3" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen to report them to the authorities.\nThese guys turn out to be the wanted criminals after all.\nYour tribe gains importance in the African community because you turned these criminals into the authorities.\nDo you want to go hunt The Mighty Lion?\n\n1- Yes; 2- No\nMake your choice " + name + ":")
		fmt.Scan(&hunter_10)
	}

	if hunter_7 == "1" {
		the_mighty_lion(name)
	}

	if hunter_7 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen not to hunt The Mighty Lion.\nUnfortunately, you die from malaria.\n" + death_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}

	if hunter_8 == "1" {
		the_mighty_lion(name)
	}

	if hunter_8 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen not to hunt The Mighty Lion.\nUnfortunately, a random lightning bolt from out of nowhere, probably from Zeus, kills you.\n" + death_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}

	if hunter_9 == "1" && rand == 1 {
		fmt.Print("\033[H\033[2J")
		fmt.Println("Unfortunately, you don't escape.\nThey kill you on the spot with a feather.\n" + death_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}

	if hunter_9 == "1" && rand == 2 {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You manage to escape.\nYou live a long and prosperous life!\n" + victory_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}

	if hunter_10 == "1" {
		the_mighty_lion(name)
	}

	if hunter_10 == "2" {
		fmt.Print("\033[H\033[2J")
		fmt.Println("You have chosen not to hunt The Mighty Lion.\nUnfortunately, a Chandra Planeswalker burns you to death!\n" + death_level)
		fmt.Scan(&pause)
		volor_savanna_original(name)
	}
}

func the_mighty_lion(name string) {
}

func main() {
	user_name()
}
