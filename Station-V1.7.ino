//Umweltdaten und Laermmesssensor der BI-LZH mit OLED
#include <WiFiManager.h>
//#include <WiFiClient.h>
//#include <WiFiClientSecure.h>
#include <InfluxDbClient.h>
#include "Adafruit_BME680.h"
#include "SdsDustSensor.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADS1X15.h>
#include <Update.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//OLED-Groesse festlegen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 

//Deklaration des SSD1306 display verbunden ueber I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//==============================================================
//                     Festlegung der globalen Variablen
//==============================================================
String device, sname, snr, sversion, ssid, wpakey, tempoffset, sealevel, dname, idbtoken, idborg, idbbucket, settings, delwifiok, updates, info, ip;
int shakepinstate = 0; 
int display_available = 1;
int period = 30000;
unsigned long time_now = 0;

#define shakepin 25

char myIpString[24];

//Setzen der Zeitzone, falls SSL Verwendet wird ist dies notwendig
// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

//==============================================================
//                     SSL-Zertifikat wird nicht benoetigt
//==============================================================
const char* root_ca = "";

//==============================================================
//                     Webserverconfig
//==============================================================
//Festlegung des Style fuer die Weboberflaeche
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:380px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

//Fetlegung der delwifi Website
String wifi = 
"<meta http-equiv=content-type content=text/html; charset=utf-8>"
"<form action=delwifi>"
"<h1>Station WiFi-Settings</h1>"
"<h2>WLAN Zugangsdaten wirklich löschen?</h2>"
"<h2>Im Anschluss bitte den Anweisungen auf dem Display folgen.</h2>"
"<input type=submit onclick=check(this.form) class=btn value=Löschen></form>"
"<form action=info>"
"<input type=submit onclick=check(this.form) class=btn value=Zurück></form>"
"<script>"
"</script>" + style;

//Festlegung der saved Website
String saved = 
"<meta http-equiv=content-type content=text/html; charset=utf-8>"
"<form action=saved>"
"<h1>Station-Einstellungen wurden erfolgreich gesetzt!</h1>"
"<h2>Die Station startet nun neu, danach sind die Einstellungen übernommen.</h2></form>"
"<script>"
"</script>" + style;

//Webserver auf Port 80 legen
WebServer server(80);

//Flashspeicher initialisieren
Preferences preferences;

//Verschluesselung zur uebertragung der Daten, noch nicht aktiv fehlende SSL Unterstuetzung
//WiFiClientSecure client2;

//Influx client Instanz erstellen
InfluxDBClient client;

//Setzen der Software-Serial-Pins für GPS
#define RX2 32 //GPS-RX2 alt34
#define TX2 33 //GPS-TX2 alt35

//Anlegen der Instanzen fuer die Sensoren
Adafruit_ADS1115 ads;
Adafruit_BME680 bme;
SdsDustSensor sds(Serial2);
TinyGPSPlus gps;
HardwareSerial SerialGPS(1);

//Double Struct fuer die Daten des GPS-Empfaengers (Vorgabe der Library)
struct GpsDataState_t {
  double originLat = 0;
  double originLon = 0;
  double originAlt = 0;
  double distMax = 0;
  double dist = 0;
  double altMax = -999999;
  double altMin = 999999;
  double spdMax = 0;
  double prevDist = 0;
};
GpsDataState_t gpsState = {};

//Einstellen des ADS1115 auf den richtigen Messbereich
int adc;
float maxvolt = 6.144, volt, vdd, dbvalue = 0.0;

//Funktion zum loeschen der WLAN-Zugangsdaten
void delWifi(){
  preferences.remove("ssid");
  preferences.remove("wpakey");
  preferences.end();
  Serial.println("WLAN-Daten erfolgreich geloescht!");
  server.sendHeader("Connection", "close");
  //Rufe die Seite delwifiok auf
  server.send(200, "text/html", delwifiok);
  delay(10000);
  ESP.restart();
}

//Funktion zur speicherung der korrektur Einstellungen
void save(){
  tempoffset = server.arg("tempoffset");
  if (tempoffset != ""){//Temperatur Offset z.B. 4.2
    preferences.putString("tempoffset", tempoffset);
    Serial.println("Temperatur-Offset erfolgreich gespeichert!");
  }else{
    preferences.putString("tempoffset", "4.2");
    Serial.println("Temperatur-Offset erfolgreich gespeichert!");
  }
  preferences.end();
  server.sendHeader("Connection", "close");
  //Rufe die Seite /saved auf
  server.send(200, "text/html", saved);
  delay(10000);
  ESP.restart();
}

//Funktion zum Auslesen des Status des Shakepin Sensors
bool read_shakepin(){
  if (digitalRead(shakepin) == HIGH){
    return true;
  }else{
    return false;
  }
}

//==============================================================
//                           SETUP
//==============================================================
void setup() {
  Serial.begin(9600);
  delay(250);
  
  //Display starten
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println("Kein Display erkannt, starte ohne Displaykonfiguration");
    //setze variable display_available auf 0 wenn kein display erkannt
    display_available = 0;
    for(;;);
  }
  //if abfrage wenn display_available == 0, dann ueberspringen, sonst initialisieren
  if(display_available == 1){
    display.clearDisplay();
    display.display();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(16, 20);
    display.setTextSize(2);
    display.println("-Starte-");
    display.display();
  }
  delay(500);
  
  //Serielle Schnittstelle fuer den GPS-Sensor einrichten
  SerialGPS.begin(9600, SERIAL_8N1, RX2, TX2);
  delay(250);
  
  //ADC starten
  ads.begin();
  delay(250);
  
  //SDS01 Starten
  sds.begin();
  delay(250);
    
  //Ausgabe der SDS01 Fiirmwareversion
  Serial.println("");
  Serial.println(sds.queryFirmwareVersion().toString()); 
  delay(50);
  //Serial.println(sds.setQueryReportingMode().toString());
  //Serial.println(sds.setActiveReportingMode().toString());
  //Serial.println(sds.setContinuousWorkingPeriod().toString());
  delay(50);
  //Temperatursensor starten
  bme.begin();
  delay(50);
  //EEPROM Einlesen
  preferences.begin("S-Settings", false);
  //ADS Gain eistellen
  ads.setGain(GAIN_ONE);
  //Festlegung des Shakepin Pins
  pinMode(shakepin, INPUT);
  
  //Auslesen der Einstellungen aus dem Flashspeicher
  sname = preferences.getString("sname", "");
  snr = preferences.getString("snr", "");
  dname = preferences.getString("dname", "");
  dname = dname + ":8086";
  idbtoken = preferences.getString("idbtoken", "");
  idborg = preferences.getString("idborg", "");
  idbbucket = preferences.getString("idbbucket", "");
  sversion = preferences.getString("sversion", "");
  ssid = preferences.getString("ssid", "");
  wpakey = preferences.getString("wpakey", "");
  tempoffset = preferences.getString("tempoffset", "");
  sealevel = preferences.getString("sealevel", "");
  //Wenn das tempoffset 0.00 oder leer ist, setze es auf 4.2 (Standardwert)
  if(tempoffset == "0.00" || tempoffset == ""){
    tempoffset = "4.2";
  }
  //Wenn das sealevel 0.00 oder leer ist, setze es auf 1013.25 (Standardwert)
  if(sealevel == "0.00" || sealevel == ""){
    sealevel = "1013.25";
  }
  //Ausgabe diverser Infos beim Start
  Serial.println("Daten aus dem Speicher:");
  Serial.println(sname.c_str());
  Serial.println(snr);
  Serial.println(sversion);
  Serial.println(dname.c_str());
  Serial.println(idbtoken.c_str());
  Serial.println(idborg.c_str());
  Serial.println(idbbucket.c_str());
  Serial.println(ssid);
  Serial.println(wpakey);
  Serial.println(tempoffset);
  Serial.println(sealevel);
  Serial.println("-----------------------------------");

  //Wenn keine Anmeldedaten fuer das WLAN vorhanden sind, setze AP-Mode und warte auf Benutzereingabe
  if (ssid == ""){
    Serial.println("Keine Anmeldedaten vorhanden!");
    WiFiManager wifiManager;
    WiFi.mode(WIFI_STA); //Aceespoint-Modus
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(sname.c_str());
    Serial.println("Accesspoint wird gestartet");
    //Starte WiFiManager im AP-Mode
    while(WiFi.status() != WL_CONNECTED) {
      if(display_available == 1){
        display.clearDisplay();
        display.display();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 1);
        display.print("IP: 192.168.4.1");
        display.setCursor(0, 11);
        display.print("SSID:    ");
        display.setCursor(35, 11);
        display.println(sname.c_str());
        display.setCursor(0, 20);
        display.println("WLAN-Key: bi-lzh.de");
        display.setCursor(0, 40);
        display.println("Bitte verbinden Sie");
        display.setCursor(0, 50);
        display.println("sich mit diesem WLAN!");
        display.display();
      }
      wifiManager.startConfigPortal(sname.c_str(), "bi-lzh.de");
      //Serial.print("IP-Adresse: ");Serial.println(WiFi.localIP());

      //Nutze MDNS zur Namensaufloesung (Station_vwxyz)
      if (MDNS.begin(sname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("MDNS gestartet!");    
      }
    }
    //Bei erfolgreicher Verbindung zum WLAN speichere die Benutzereingaben im Flash
    ssid = WiFi.SSID();
    wpakey = WiFi.psk();
    delay(50);
    preferences.putString("ssid", ssid.c_str());
    preferences.putString("wpakey", wpakey.c_str());
    Serial.println("WLAN-Daten in Speicher geschrieben");
    preferences.end();
    if(display_available == 1){
      display.clearDisplay();
      display.display();
    }
    delay(500);
    wifiManager.resetSettings();
    delay(50);
    ESP.restart();
  }
  //Wenn WLAN-Daten vorhanden sind versuche Aufbau der Verbindung
  else{
    int i = 0;
    //Lese die gespeicherten Verbindungsdaten aus dem Flash
    Serial.println("");
    Serial.println("Dies sind die gespeicherten Zugangsdaten zum Accesspoint: ");
    Serial.print("SSID: ");Serial.println(ssid);
    Serial.print("WPA-Key: ");Serial.println(wpakey);
    Serial.println("");
    //sname = sname.c_str();
    //bool setHostname(const char * sname);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(sname.c_str());
    WiFi.begin(ssid.c_str(), wpakey.c_str());
    //WiFi.setHostname(sname.c_str());
    //Nutze MDNS zur Namensaufloesung (Station_vwxyz)
    if (MDNS.begin(sname.c_str())) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("MDNS gestartet!");
     }
    delay(200);
    //Stelle die Verbindung mit den gespeicherten Zugangsdaten her
    Serial.print("Verbindung mit SSID: ");Serial.print(ssid);
    Serial.print(" und Password: ");Serial.print(wpakey);
    Serial.print(" wird aufgebaut ");
    //Warte bis Verbindung aufgebaut
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      i++;
      
      //NEU Test wenn WLAN nicht vorhanden und Shakepin == True, dann starte Captive-Portal -> alt delay(500);
      time_now = millis(); 

      while(millis() < time_now + 1000){
        if (read_shakepin()){
          shakepinstate = 1;
        }
      }
      if (shakepinstate == 1){
        Serial.println("WLAN nicht gefunden, shakepin = True");
        WiFiManager wifiManager;
        WiFi.mode(WIFI_STA); //Aceespoint-Modus
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
        WiFi.setHostname(sname.c_str());
        Serial.println("Accesspoint wird gestartet");
        //Starte WiFiManager im AP-Mode
        while(WiFi.status() != WL_CONNECTED) {
          if(display_available == 1){
            display.clearDisplay();
            display.display();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, 1);
            display.print("IP: 192.168.4.1");
            display.setCursor(0, 11);
            display.print("SSID:    ");
            display.setCursor(35, 11);
            display.println(sname.c_str());
            display.setCursor(0, 20);
            display.println("WLAN-Key: bi-lzh.de");
            display.setCursor(0, 40);
            display.println("Bitte verbinden Sie");
            display.setCursor(0, 50);
            display.println("sich mit diesem WLAN!");
            display.display();
          }
          wifiManager.startConfigPortal(sname.c_str(), "bi-lzh.de");
          //Serial.print("IP-Adresse: ");Serial.println(WiFi.localIP());

          //Nutze MDNS zur Namensaufloesung (Station_vwxyz)
          if (MDNS.begin(sname.c_str())) {
            MDNS.addService("http", "tcp", 80);
            Serial.println("MDNS gestartet!");    
          }
        }
        //Bei erfolgreicher Verbindung zum WLAN speichere die Benutzereingaben im Flash
        ssid = WiFi.SSID();
        wpakey = WiFi.psk();
        delay(50);
        preferences.putString("ssid", ssid.c_str());
        preferences.putString("wpakey", wpakey.c_str());
        Serial.println("WLAN-Daten in Speicher geschrieben");
        preferences.end();
        if(display_available == 1){
          display.clearDisplay();
          display.display();
        }
        delay(500);
        wifiManager.resetSettings();
        delay(50);
        ESP.restart();
      }
      // ENDE NEU

      //nach 30Sek starte die Station neu-----WLAN-Daten loeschen und AP aufbauen waere noch eine moegliche Zusatzoption
      if(i == 60){
        ESP.restart();
      }
    }
    Serial.println("");
    Serial.println("Verbindung zum WLAN aufgbaut!");
    Serial.print("IP-Adresse: ");Serial.println(WiFi.localIP()); //WiFi.softAPIP();
    //Speichere aktuelle IP-Adresse in IPAdress
    IPAddress myIp = WiFi.localIP();
    //Formatierung der IP-Adresse
    ip = String(myIp[0]) + "." + String(myIp[1]) + "." + String(myIp[2]) + "." + String(myIp[3]);
    //Vorbereitung Display und Ausgabe der Infodaten
    if(display_available == 1){
      display.clearDisplay();
      display.display();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 1);
      display.print("IP:");
      display.setCursor(21, 1);
      display.println(ip);
      display.setCursor(0, 10);
      display.print("Name:");
      display.setCursor(35, 10);
      display.println(sname.c_str());
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
    }
  }
  //Pruefe ob Stationname erfolgreich angelegt als MDNS
  if (!MDNS.begin(sname.c_str())) { //http://Station_x.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  
  //Festlegung der info Website
  info = 
  "<meta http-equiv=content-type content=text/html; charset=utf-8>"
  "<form>"
  "<h1>Willkommen auf Iher " + sname + "</h1>"
  "<h2>Unter folgenden Links können Sie Einstellungen vornehmen.</h2>"
  "<h4>Version " + sversion + "</h4>"
  "</form>"
  "<form action=settings>"
  "<h2>1. Hier können Sie den Temparaturoffset neu einstellen.</h2>"
  "<input type=submit onclick=check(this.form) class=btn value=Einstellungen>"
  "</form>"
  "<form action=wifi>"
  "<h2>2. Hier können Sie die WLAN-Zugangsdaten löschen und in den Einrichtungsmodus wechseln.</h2>"
  "<input type=submit onclick=check(this.form) class=btn value=WLAN>"
  "</form>"
  "<form action=update>"
  "<h2>3. Hier können Sie ein Update der Station durchführen.</h2>"
  "<input type=submit onclick=check(this.form) class=btn value=Update>"
  "</form>"
  "<script>"
  "</script>" + style;
  
  //Festlegung der delwifiok Website
  delwifiok = 
  "<meta http-equiv=content-type content=text/html; charset=utf-8>"
  "<form action=delwifiok>"
  "<h1>Station WiFi-Settings wurden erfolgreich gelöscht!</h1>"
  "<h2>Bitte melden Sie sich nun an dem neuen WLAN " + sname + " an, um das WLAN Ihrer Station neu einzurichten.</h2>"
  "<h4>Tipp: schauen Sie hierzu auf das Display auf der Unterseite</h4></form>"
  "<script>"
  "</script>" + style;

  //Festlegung der settings Website
  settings = 
  "<meta http-equiv=content-type content=text/html; charset=utf-8>"
  "<form action=save>"
  "<h1>Station Settings</h1>"
  "<h2>IP: " + ip + "</h2>"
  "<br><br>"
  "<h2>Derzeit gesetztes Temperatur Offset: " + tempoffset + "°C</h2>"
  "<h2>Default Temperatur Offset: 4.2°C</h2>"
  "<h4>Dieser Wert wird von dem gemessenen Wert abgezogen.</h4>"
  "<input name=tempoffset placeholder='Temperatur Offset z.B. 4.2'> "
  "<input type=submit onclick=check(this.form) class=btn value=Speichern></form>"
  "<form action=info>"
  "<input type=submit onclick=check(this.form) class=btn value=Zurück></form>"
  "<script>"
  "</script>" + style;

  //Festlegung der updates Website inkl JS-Script fuer die Animation beim Upload/Flash
  updates = 
  "<meta http-equiv=content-type content=text/html; charset=utf-8>"
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<h1>Station Update</h1>"
  "<h3>Achten Sie auf die richtige Dateiendung! (.bin)</h2>" 
  "<p>Installierte Version " + sversion + "</p>"
  "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
  "<label id='file-input' for='file'>   Datei wählen...</label>"
  "<input type='submit' class=btn value='Update starten'>"
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
  "$('#prg').html('Fortschritt: ' + Math.round(per*100) + '%');"
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
  
  Serial.println("Lade Konfiguration fuer Influxdb");
  //Influx Client konfigurieren
  client.setConnectionParams(dname.c_str(), idborg.c_str(), idbbucket.c_str(), idbtoken.c_str());

  delay(50);
  //wifiManager.resetSettings(); alt kann weg
  
  Serial.println("Warte auf eingaben im Browser");
  //Bei Aufruf der Seite / rufe Seite Info auf
  server.on("/", HTTP_GET, []() {
    //server.sendHeader("Connection", "close");
    server.send(200, "text/html", info);
  });
  //Bei Aufruf der Seite /info rufe Seite Info auf
  server.on("/info", HTTP_GET, []() {
    //server.sendHeader("Connection", "close");
    server.send(200, "text/html", info);
  });
  //Bei Aufruf der Seite /settings rufe Seite Settings auf
  server.on("/settings", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", settings);
  });
  //Bei Aufruf der Seite /save rufe Seite Save auf
  server.on("/save", save);
  //Bei Aufruf der Seite /wifi rufe Seite Wifi auf
  server.on("/wifi", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", wifi);
  });
  //Bei Aufruf der Seite /delwifi rufe Seite delwifi auf
  server.on("/delwifi", delWifi);
  //Bei Aufruf der Seite /update rufe Seite Updates auf
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updates);
  });
  //Bei Aufruf der Seite /update fuehre das Update durch
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
    Serial.print("Update: "); Serial.println(sversion);
    preferences.putString("sversion", sversion);
    Serial.println("Station-Version in Speicher geschrieben");
    preferences.end();
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  
  //Starte den Webserver
  Serial.println("Starte Webserver");
  server.begin();

  //client2.setCACert(root_ca); //falls Benutzung gewuenscht auskommentieren derzeit keine SSL Unterstuetzung
  
  delay(5000);
  //Schalte Beleuchtung des OLED wieder aus
  if(display_available == 1){
    display.clearDisplay();
    display.display();
  }
  //Initialisiere BME680
  bme.setTemperatureOversampling(BME680_OS_8X); //nochmal Bibliotheck pruefen
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
  }
      
//==============================================================
//                     LOOP
//==============================================================
void loop(void) {

  //Aufruf der Funktion waitforclient zum Aufruf der Weboberflaeche
  waitforclient();
  //Aufruf der Funktion read_sds fuer die Feinstaubmessung
  read_sds();
  //Aufruf der Funktion waitforclient zum Aufruf der Weboberflaeche
  waitforclient();  
  //Aufruf der Funktion read_bmeandgps fuer das auslesen des Temperatur und GPS-Sensors
  read_bmeandgps();
  //Aufruf der Funktion waitforclient zum Aufruf der Weboberflaeche
  waitforclient();
  
  //Beginn der Lautstaerkemessung fuer insg. 5Min
  Serial.println("Messe Lautstaerke");
  for (int count = 0; count < 10; count++) {
    //Aufruf der Funktion readdb fuer die Lautstaerkemessung
    readdb();
    Serial.print("Anzahl an durchlaeufen der dB Messung: ");Serial.println(count);
  }
}

//Funktion waitforclient welche den Webserver anweist auf eine Clientverbindung zu warten
void waitforclient(){
  time_now = millis();
  while(millis() < time_now + 250){
    server.handleClient();
  }
}

//Funktion readdb zum Auslesen des Lautstaerkesensors
void readdb(){
  Point sensor(sname.c_str()); 
  sensor.clearFields();
  sensor.addTag("Serialnumber", snr.c_str());
  //Festlegung der benoetigten Variablen
  float sum, adv, volts0;
  int16_t adc0;
  float  dbvaluemax = 0, dbvaluemin = 130, dbvalueavg = 0;
    //Pruefe ob Datenbankverbindung erfolgreich
    if (client.validateConnection()) {
      Serial.println("Verbindung zur Datenbank erfolgreich");
        for (int counti = 0; counti < 300; counti++) {// fuer 30 sekunden messen
          time_now = millis();
          //Pruefe ob ein Client eine Verbindung zum Webserver aufbauen will 
          while(millis() < time_now + 100){
              server.handleClient();
              //Pruefe ob der Shakepin 1 ist
              if (read_shakepin()){
                shakepinstate = 1;
              }
          }
          //Lese den Kanal 0 des ADS1115 aus
          adc0 = ads.readADC_SingleEnded(0);
          //Erzeuge Volt Messwerte
          volts0 = ads.computeVolts(adc0);
          //Berechne das dB Value
          dbvalue = volts0 * 50.0;
          //Zeige aktuellen Messwert in der Konsole
          Serial.println("-------------------------------");
          Serial.print("AIN0: "); Serial.print(adc0); Serial.print("  "); Serial.print(volts0); Serial.print("V"); Serial.print("  "); Serial.print(dbvalue); Serial.println("dBA");
          //Fuege der Variable dbvalueavg den aktuellen Messwert hinzu 
          dbvalueavg = dbvalueavg + dbvalue;
          //Alle 100ms wird verglichen, ob ein gemessener Wert groesser oder kleiner dem vorherigen ist somit wird bestimmt welches der lauteste oder leiseste Messwert ist
          if (dbvalue >= dbvaluemax)
          {
            dbvaluemax = dbvalue;
          }
          if (dbvalue < dbvaluemin)
          {
            dbvaluemin = dbvalue;
          }
          //Wenn Shakepin true und Display ist verfuegbar zeige folgendes an
          if (shakepinstate == 1 && display_available == 1){
            //Zeige aktuellen L-Messwert auf OLED
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, 1);
            display.println("IP:");
            display.setCursor(21, 1);
            display.println(ip);
            display.setTextSize(2);
            display.setCursor(0, 15);
            display.print("Aktuell:");
            display.setCursor(0, 35);
            display.println(dbvalue);
            display.setCursor(55, 35);
            display.print(" dBA");
            display.display(); 
          }
          Serial.print("L-Counter: "); Serial.println(counti);
        }
      if(display_available == 1){
        //Schalte Beleuchtung des OLED wieder aus
        display.clearDisplay();
        display.display();
      }
      //Setze den Shakepin fuer den naechsten Durchlauf auf 0
      shakepinstate = 0;
      //Zeige alle festgelegten Messwerte der Messreie noch mal in der seriellen Konsole
      Serial.print("Aktuelle Lautstaerke gemessen: ");
      Serial.print(dbvalue); Serial.println("dBA");
      dbvalueavg = (dbvalueavg/300);//durch 300 Messungen teilen
      Serial.print("dbvaluemax = ");Serial.println(dbvaluemax);
      Serial.print("dbvaluemin = ");Serial.println(dbvaluemin);
      Serial.print("dbvalueavg = ");Serial.println(dbvalueavg);
      Serial.print("dbvalue    = ");Serial.println(dbvalue);
      sensor.clearFields();
      sensor.addField("dBAmax", dbvaluemax);
      sensor.addField("dBAmin", dbvaluemin);
      sensor.addField("dBAavg", dbvalueavg);
      sensor.addField("dBA", dbvalue);
      client.writePoint(sensor);
      Serial.println("Lautstaerkemessungen erfolgreich in Datenbank geschrieben!");
    }
    //Wenn keine Verbindung zur Datenbank, dann starte WLAN neu und warte 10sek
    else {
      int count = 0;
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
      while (client.getLastErrorMessage() == "connection refused"){
        WiFi.reconnect();
        //Wenn das Display verfuegbar ist, zeige an, dasss die Verbindung zur Datenbank fehlgeschlagen ist
        if(display_available == 1){
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(WHITE);
          display.setCursor(0, 1);
          display.println("Keine Verbindung");
          display.setCursor(0, 11);
          display.println("zur Datenbank!");
          display.setCursor(0, 21);
          display.println("Warte auf");
          display.setCursor(0, 31);
          display.println("Verbindung...");
          display.display();
        }
        time_now = millis();
        //Pruefe ob ein Client eine Verbindung zum Webserver aufbauen will
        while(millis() < time_now + 10000){ 
          server.handleClient();
        }
        //Falls das Display verfuegbar war, schalte es aus
        if(display_available == 1){
          display.clearDisplay();
          display.display();
        }
        //Zaehle die Verbindungsversuche
        count++;
        Serial.println(count);
        client.validateConnection();
        client.getLastErrorMessage();
        Serial.println(client.getLastErrorMessage());
        if(count == 60){
          count = 0;
          delay(500);
          //ESP.restart(); Derzeit auskommentiert, da die Box sonst haeufig neustartet!...?
        }
      }
    }
}

//Funktion read_sds liesst den Feinstaubsensor aus
void read_sds(){
  Point sensor(sname.c_str()); 
  sensor.clearFields();
  sensor.addTag("Serialnumber", snr.c_str());
  //Festlegung benoetigter Variablen
  float fein25 = 0; 
  float fein10 = 0;
  shakepinstate == 0;
  //Pruefe ob Datenbankverbindung erfolgreich
  if (client.validateConnection()) {
    //Der SdsDustSensor011 wird gestartet
    Serial.println("Starte SDS011");
    WorkingStateResult state = sds.wakeup();
    time_now = millis();
    Serial.println("Wecke SDS auf");
    //Wenn er laeuft gib eine Info aus ansonsten ueberspringe folgendes
    if (state.isWorking()){
      Serial.println("SDS arbeitet");
      //Pruefe ob sich ein Client mit dem Webserver verbinden moechte
      while(millis() < time_now + 30000){
        server.handleClient();
        //Lese den Shakepin ein
        if (read_shakepin()){
          shakepinstate = 1;
        }
        //Wenn Shakepin 1 und das Display erreichbar ist gebe folgende Info aus
        if (shakepinstate == 1 && display_available == 1){
          //Zeige aktuellen L-Messwert auf OLED
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(WHITE);
          display.setCursor(0, 1);
          display.println("IP:");
          display.setCursor(21, 1);
          display.println(ip);
          display.setCursor(0, 15);
          display.print("Aktuell Feinstaub- ");
          display.setCursor(0, 25);
          display.print("messung, daher keine");
          display.setCursor(0, 35);
          display.print("Lautstaerkemessung!");
          display.display();
        }
      }
      //Abfrage SdsDustSensor Sensor
      PmResult pm = sds.queryPm();
      //Pruefe ob die Messwerte i.O. sind
      if (pm.isOk()) {
        fein25 = pm.pm25;
        fein10 = pm.pm10;
        Serial.println(pm.toString());
      } else {
        Serial.print("Probleme beim Lesen des Sensors, Grund: ");
        Serial.println(pm.statusToString());
      }
      //Sensor in den Standby versetzen und ueberpruefen
      WorkingStateResult statee = sds.sleep();
      if (statee.isWorking()) {
        Serial.println("Problem mit Standby des Sensors.");
      } else {
        Serial.println("Sensor im Standby");
      }
      //Messdaten werden dem Fluxquery hinzugefuegt
      sensor.addField("fein25", fein25);
      sensor.addField("fein10", fein10);
      sensor.addField("rssi", WiFi.RSSI());
      //Messdaten werden in die Datenbank geschrieben
      client.writePoint(sensor);
      Serial.println("Feinstaub erfolgreich in Datenbank geschrieben!");
      //Wenn das Display erreichbar war, schalte es aus
      if(display_available == 1){
        display.clearDisplay();
        display.display();
      }
      shakepinstate = 0;
    }else{
        Serial.println("Feinstaubsensor nicht gestartet");
      } 
    }
    //Wenn keine Verbindung zuer Datenbank, dann starte WLAN neu und warte 10sek
    else {
      int count = 0;
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
      //Pruefe ob die Verbindung zuer Datenbank funktioniert, wenn nicht starte WLAN neu
      while (client.getLastErrorMessage() == "connection refused"){
        WiFi.reconnect();
        //Wenn das Display erreichbar ist, zeige folgende Infos an
        if(display_available == 1){
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(WHITE);
          display.setCursor(0, 1);
          display.println("Keine Verbindung");
          display.setCursor(0, 11);
          display.println("zur Datenbank!");
          display.setCursor(0, 21);
          display.println("Warte auf");
          display.setCursor(0, 31);
          display.println("Verbindung...");
          display.display();
        }
        time_now = millis();
        //Pruefe ob sich ein Client mit dem Webserver verbinden will
        while(millis() < time_now + 10000){ 
          server.handleClient();
        }
        //Pruefe ob das Display verfuegbar war, dann schalte es aus
        if(display_available == 1){
          display.clearDisplay();
          display.display();
        }
        //Zaehle die Neustarts der WLAN-Verbindung
        count++;
        Serial.println(count);
        client.validateConnection();
        client.getLastErrorMessage();
        Serial.println(client.getLastErrorMessage());
        if(count == 30){
          count = 0;
          delay(1000);
          //ESP.restart(); Auskommentiert, da der ESP sonst staendig neustartet
        }
      }
    }
}

//Funktion zum Auslesen des Temperatur und GPS-Sensors
void read_bmeandgps(){
  Point sensor(sname.c_str()); 
  sensor.clearFields();
  sensor.addTag("Serialnumber", snr.c_str());
  //Festlegung der benoetigten Variablen
  float temp, hum, pres, gas, alti;
  //Abfrage ob BME bereit zum auslesen
  if (!bme.performReading()) {
    Serial.println("Fehler beim Lesen von BME680");
    return;
  } 
  else {
    //Ab hier werden die Messdaten des BME680 gelesen und in die Variablen geschrieben
    Serial.println("Lese Daten Von BME");
    Serial.println(sealevel.toFloat());
    temp = bme.temperature;//-offset
    temp = temp - tempoffset.toFloat();//Temperaturoffset von gemessener Temperatur abziehen
    hum = bme.humidity;
    pres = bme.pressure;
    gas = bme.gas_resistance;
    alti = bme.readAltitude(sealevel.toFloat()); //sealevel.toFloat()
    //Ausgabe der MEssergebnisse
    Serial.println(temp);
    Serial.println(hum);
    Serial.println(pres);
    Serial.println(gas);
    Serial.println(alti);
    sensor.addField("druck", pres / 100);
    sensor.addField("temp", temp);
    sensor.addField("feucht", hum);
    sensor.addField("gas", gas/1000.0);
    sensor.addField("alti", alti);
  }
  //GPS-Daten werden eingelesen, solange der Sensor mehr als 0 Sateliten findet  
  while (SerialGPS.available() > 0){
    gps.encode(SerialGPS.read());
  }
  Serial.println("GPS Daten gelesen");
  //GPS-Daten werden dem Query hinzugefuegt, solange der Sensor mehr als 2 Sateliten findet 
  if (gps.satellites.value() >= 2) {
    Serial.println("Ausreichend Sateliten gefunden");
    sensor.addField("Latitude", gps.location.lat(), 6);
    sensor.addField("Longitude", gps.location.lng(), 6);
    sensor.addField("Height", gps.altitude.meters());
    sensor.addField("Satelite", gps.satellites.value());
  } 
  else {
    Serial.println("Zu wenige Sateliten gefunden, versuche es in 5Min erneut");
  }
  //Pruefe die Verbindung zur Datenbank
  client.validateConnection();
  //Wenn die Verbindung nicht klappt, gebe folgenden Fehler aus
  if (client.getLastErrorMessage() == "connection refused"){
      Serial.println("Messdaten konnten nicht in die Datenbank geschrieben werden!");
      Serial.println(client.getLastErrorMessage());
  }
  //Ansonsten schreibe die Daten in die Datenbank
  else{
    client.writePoint(sensor);
    Serial.println("BME-Daten & GPS in Datenbank geschrieben!");
    Serial.print("Anzahl an Sateliten: ");Serial.println(gps.satellites.value());
  }
}
