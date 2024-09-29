#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Flashspeicher initialisieren
Preferences preferences;

//Server on port 80
WebServer server(80); 

//==============================================================
//               Festlegung der globalen Variablen
//==============================================================
//OLED-Groesse festlegen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 

//Deklaration des SSD1306 display verbunden ueber I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//SSID and Password von deinem WLAN ##### ANPASSEN #############
const char* sssid = "Test";
const char* spassword = "xkokd208zr74hf,x0fk00v934";
char myIpString[24];
//SSID and Password von deinem WLAN ##### ANPASSEN #############

//Festlegung der globalen Variablen
String sversion, updates, ip; 
String myhostname = "Station_NEU";

//==============================================================
//                     Webserverconfig
//==============================================================
//Festlegung des Style fuer die Weboberflaeche
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

// Einstellungswebsite
String settings = 
"<form action=save>"
"<h1>Station-Settings</h1>"
"<input name=sname placeholder='Station_xxxxx'> "
"<input name=dname placeholder='Domain Name'> "
"<input name=idbtoken placeholder='Influx Token'>"
"<input name=idborg placeholder='Influx Organisation'> "
"<input name=idbbucket placeholder='Influx Bucket'>"
"<input type=submit onclick=check(this.form) class=btn value=Speichern></form>"
"<script>"
"{window.open('/update')}"
"</script>" + style;

//Festlegung der Update erfolgreich Website
String updateok = 
"<meta http-equiv=content-type content=text/html; charset=utf-8>"
"<form action=updateok>"
"<h1>Update wurde erfolgreich geschrieben!</h1>"
"<h2>Die Station startet nun neu und ist in ein paar Sekunden wieder erreichbar.</h2></form>"
"<script>"
"</script>" + style;

//===============================================================
// Diese Funktionen werden durchlaufen beim Klick auf Speichern
//===============================================================
void handleForm() {
 //Festlegung der Variablen
 String sname = server.arg("sname");
 String snr = sname;
 //Entfernung des Namen aus der Seriennummer
 snr.replace("Station_", ""); 
 String dname = server.arg("dname");
 String idbtoken = server.arg("idbtoken");
 String idborg = server.arg("idborg");
 String idbbucket = server.arg("idbbucket");
 //Serial.print(idbbucket);

 //Umwandung der z.B Eingabe 8 als Seriennummer in 1008 da in der alten influxdb die Seriennummern im 1.000er Bereich lagen.
 if (strstr(sname.c_str(), "_1000") != NULL)
  {
     sname.replace("1000", "");
  }
  else if (strstr(sname.c_str(), "_100") != NULL)
  {
    sname.replace("100", "");
  }
  else if (strstr(sname.c_str(), "_10") != NULL)
  {
    sname.replace("10", "");
  }
  else if (strstr(sname.c_str(), "_1") != NULL)
  {
    sname.replace("1", "");
  }

  //Infoausgabe in der Konsole
  Serial.println("-----------------------------------");
  Serial.print("Eingelesen Station Name: ");Serial.println(sname);
  Serial.print("Eingelesen Station Number: ");Serial.println(snr);
  Serial.print("Eingelesen Domain Name: ");Serial.println(dname);
  Serial.print("Eingelesen InfluxDB Token: ");Serial.println(idbtoken);
  Serial.print("Eingelesen InfluxDB Organisation: ");Serial.println(idborg);
  Serial.print("Eingelesen InfluxDB Bucket: ");Serial.println(idbbucket);
  Serial.print("Eingelesen Station Version: ");Serial.println(sversion);

  //Speicherung der Daten im EEPROM/Flashspeicher
  preferences.putString("sname", sname);
  preferences.putString("snr", snr);
  preferences.putString("dname", dname);
  preferences.putString("idbtoken", idbtoken);
  preferences.putString("idborg", idborg);
  preferences.putString("idbbucket", idbbucket);
  preferences.putString("tempoffset", "4.2");
  preferences.putString("sealevel", "1013.25");
  Serial.println("Station-Settings in Speicher geschrieben");
  Serial.println("-----------------------------------");
  
  preferences.end();
  //Beendigung der Client-Server Verbindung
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", updates);
  delay(100);
  //Neustart des Mikrokontrollers
  ESP.restart();
}

//==============================================================
//                           SETUP
//==============================================================
void setup(void){
  //Serielle Schnittstelle konfigurien und bereitstellen
  Serial.begin(115200);
  //Display starten
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Addresse 0x3C fuer 128x64
    Serial.println(F("Display nicht gefunden"));
    for(;;); // weg für Projekt
  }
  //Display loeschen
  display.clearDisplay();
  display.display();
  delay(500);
  
  //Auslesen des EEPROM Starten
  preferences.begin("S-Settings", false);
  //Zum debuggen EEPROM loeschen
  //preferences.clear();
  //Serial.println("Speicher geloescht");

  //WLAN-Konfiguration einlesen
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE); //was ist das?
  //Hostnamen setzen
  WiFi.setHostname(myhostname.c_str());
  //WLAN Starten
  WiFi.begin(sssid, spassword);
  Serial.println("");

  //Warte auf erfolgreiche WLAN Verbindung
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  

  //Nach erfolgreicher Verbindung mit dem WLAN Info in der Konsole ausgeben
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println("WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //IP-Adresse bestimmen
  IPAddress myIp = WiFi.localIP();
  //IP-Adresse Formatieren
  ip = String(myIp[0]) + "." + String(myIp[1]) + "." + String(myIp[2]) + "." + String(myIp[3]);
  //Display einrichten
  display.setTextSize(1);
  display.setTextColor(WHITE);
  //Ausgbae der Grundinformationen
  display.setCursor(0, 1);
  display.print("IP:");
  display.setCursor(21, 1);
  display.println(ip);
  display.setCursor(0, 10);
  display.print("        Station");
  display.setCursor(0, 23);
  display.println(" ------------------- ");
  display.setCursor(0, 26);
  display.println("|                   |");
  display.setCursor(0, 31);
  display.println("|        www.       |");
  display.setCursor(0, 41);
  display.println("|     bi-lzh.de     |");
  display.setCursor(0, 50);
  display.println("| stations.bi-lzh.de|");
  display.setCursor(0, 57);
  display.println("|                   |");
  display.setCursor(0, 60);
  display.println(" ------------------- ");
  display.display();
  //EEPROM Auslesen
  String sname = preferences.getString("sname", "");
  String snr = preferences.getString("snr", "");
  String dname = preferences.getString("dname", "");
  String idbtoken = preferences.getString("idbtoken", "");
  String idborg = preferences.getString("idborg", "");
  String idbbucket = preferences.getString("idbbucket", "");
  sversion = preferences.getString("sversion", "");

  //Festlegung der updates Website inkl JS-Script fuer die Animation beim Upload/Flash
  updates = 
  "<meta http-equiv=content-type content=text/html; charset=utf-8>"
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<h1>Station Update</h1>" 
  "<p>Installierte Version " + sversion + "</p>"
  "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
  "<label id='file-input' for='file'>   Datei wählen...</label>"
  "<input type='submit' class=btn value='Update'>"
  "<br><br>"
  "<div id='prg'></div>" 
  "<br><div id='prgbar'><div id='bar'></div></div><br></form>"
  "<script>"
  "function sub(obj){"
  "var fileName = obj.value.split('\\\\');"
  "document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
  "};"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  "$.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "$('#bar').css('width',Math.round(per*100) + '%');" 
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!') "
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>" + style;
  
  //Ausgabe der gespeicherten Informationen im Terminal
  Serial.print("Station Name: ");Serial.println(sname);
  Serial.print("Station Number: ");Serial.println(snr);
  Serial.print("Domain Name: ");Serial.println(dname);
  Serial.print("InfluxDB Token: ");Serial.println(idbtoken);
  Serial.print("InfluxDB Organisation: ");Serial.println(idborg);
  Serial.print("InfluxDB Bucket: ");Serial.println(idbbucket);
  Serial.print("Station Version: ");Serial.println(sversion);
  
  //Nutze MDNS zur Namensaufloesung
  if (!MDNS.begin(myhostname.c_str())) { //http://Station_NEU.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  //Wenn die Weboberflaeche mit / aufgerufen wird, rufe die Seite Settings auf
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", settings);
  });
  //Wenn die Weboberflaeche mit /update aufgerufen wird, rufe die Seite updates auf
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updates);
  });
  //Wenn die Weboberflaeche mit /save aufgerufen wird, rufe die Seite save auf
  server.on("/save", handleForm);
  //Wenn die Weboberflaeche mit /update aufgerufen wird, rufe die Seite updates auf
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
	  sversion = upload.filename.c_str();
	  sversion.replace("Station-", "");
	  sversion.replace(".bin", "");
    Serial.print("Update: ");Serial.println(sversion);
	  preferences.putString("sversion", sversion);
	  Serial.println("Station-Version in Speicher geschrieben");
	  preferences.end();
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //starte mit maximaler Dateigroesse
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashen der firmware auf ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true wenn die uebertragene Groesse zum Prozessbalken uebertragen werden soll
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
		//Mal testen!!!!
		//server.sendHeader("Connection", "close");
		//server.send(200, "text/html", updateok);
      } else {
        Update.printError(Serial);
      }
    }
  });

  //Start des Webservers
  server.begin();
  Serial.println("HTTP server started");
}

//==============================================================
//                     LOOP
//==============================================================
void loop(void){
  //Warten auf Clientverbindung
  server.handleClient();
}
