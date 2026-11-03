function reset_local_storage()
{
    localStorage.clear();
}

function ask_name()
{
    user_name = localStorage.getItem("name");

    if (user_name == null)
    {
        document.getElementById("my_text").innerHTML = "What is your name?<br>";
        let choice = document.getElementById("choice_field").value;
        if (choice.length > 0)
        {
            localStorage.setItem("name", choice);
            localStorage.setItem("death_level", choice + ", YOU LOSE!<br>THE END!");
            localStorage.setItem("victory_level", choice + ", YOU WIN!<br>THE END!");
        }
    }
    
    else
    {
        volor_savanna_original()
    }
}

function volor_savanna_original()
{
    document.getElementById("my_text").innerHTML = "Welcome to Volor Savanna!<br>You are a member of an African Tribe!<br>Who do you want to be?<br><br>1- Hunter<br>2- Warrior<br>3- Crafter<br>4- Farmer<br>5- Medicine Person<br>e- exit<br>";
    let choice = document.getElementById("choice_field").value;
}