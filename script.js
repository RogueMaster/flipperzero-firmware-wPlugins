function reset_local_storage()
{
    localStorage.clear();
}

function ask_name()
{
    let user_name = localStorage.getItem("user_name");

    if (user_name == null)
    {
        document.getElementById("my_text").innerHTML = "What is your name?<br>";
        let user_input = document.getElementById("choice_field").value;
        if (user_input.length > 0)
        {
            localStorage.setItem("user_name", user_input);
            localStorage.setItem("death_level", user_input + ", YOU LOSE!<br>THE END!");
            localStorage.setItem("victory_level", user_input + ", YOU WIN!<br>THE END!");
        }
    }
    
    else
    {
        volor_savanna_original();
    }
}

function volor_savanna_original()
{
    let character = localStorage.getItem("character");
    
    if (character == null)
    {
        document.getElementById("my_text").innerHTML = "Welcome to Volor Savanna!<br>You are a member of an African Tribe!<br>Who do you want to be?<br><br>1- Hunter<br>2- Warrior<br>3- Crafter<br>4- Farmer<br>5- Medicine Person<br>e- exit<br>";
        let user_input = document.getElementById("choice_field").value;
        if (user_input.length > 0)
        {
            localStorage.setItem("character", user_input);
        }
    }

    else
    {
         if (character == "1")
        {
            hunter();
        }

        if (character == "2")
        {
            warrior();
        }

        if (character == "3")
        {
            crafter();
        }

        if (character == "4")
        {
            farmer();
        }

        if (character == "5")
        {
            medicine_person();
        }
    }
}

function hunter()
{
    let rand = Math.floor(Math.random() * 2 ) + 1;

    let user_name = localStorage.getItem("user_name");
    let death_level = localStorage.getItem("death_level");
    let victory_level = localStorage.getItem("victory_level");
    let choice = localStorage.getItem("choice");
    let idx = localStorage.getItem("idx");
    
    if (choice == null)
    {
        document.getElementById("my_text").innerHTML = "You have chosen to be a hunter!<br>Do you want to go hunt or stay put?<br><br>1- Hunt; 2- Stay put<br>Make your choice " + user_name + ":<br>";
        let user_input = document.getElementById("choice_field").value;
        if (user_input.length > 0)
        {
            localStorage.setItem("idx", "hunter_1");
            localStorage.setItem("choice", user_input);
        }
    }

    else
    {
        if (choice.length > 0)
        {
            if (idx == "hunter_1" && choice == "1")
            {
                document.getElementById("my_text").innerHTML = "You have chosen to go hunt!<br>You have gone hunting.<br>You didn't find any food.<br>You return to your village.<br>Luckily, the farmer has enough crops to last your tribe a week.<br>Do you want to go hunt right away to make up lost time or do you choose to go eat?<br><br>1- Hunt; 2- Eat<br>Make your choice " + user_name + ":<br>";
                user_input = document.getElementById("choice_field").value;
                document.querySelector("form").addEventListener("submit", function(event)
                {
                    localStorage.setItem("idx", "hunter_2");
                    localStorage.setItem("choice", user_input);
                    location.reload();
                });
            }

            if (idx == "hunter_1" && choice == "2")
            {
                document.getElementById("my_text").innerHTML = "You have chosen to stay put.<br>Unfortunately, your whole village parishes from hunger.<br>" + death_level;
                user_input = document.getElementById("choice_field").value;
                document.querySelector("form").addEventListener("submit", function(event)
                {
                    localStorage.removeItem("character");
                    localStorage.removeItem("choice");
                    localStorage.removeItem("idx");
                    location.reload();
                });
            }

            if (idx == "hunter_2" && choice == "1")
            {
                document.getElementById("my_text").innerHTML = "You have chosen to go hunt!<br>You have gone hunting.<br>You didn't find any food.<br>You return to your village.<br>Luckily, the farmer has enough crops to last your tribe a week.<br>Do you want to go hunt right away to make up lost time or do you choose to go eat?<br><br>1- Hunt; 2- Eat<br>Make your choice " + user_name + ":<br>";
                user_input = document.getElementById("choice_field").value;
                document.querySelector("form").addEventListener("submit", function(event)
                {
                    localStorage.setItem("idx", "hunter_3");
                    localStorage.setItem("choice", user_input);
                    location.reload();
                });
            }
        }
    }
}