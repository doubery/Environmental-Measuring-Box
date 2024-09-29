# Environmental-Measuring-Box
ESP32-based environmental measuring box for volume, temperature, humidity, air humidity, particulate matter, GPS

All measured data are sent to an influx database and in my case analyzed and displayed using grafana.

# Circuit diagram

![alt text](https://github.com/doubery/Environmental-Measuring-Box/blob/main/images/BI-Sensor_Neu_Steckplatine.png)

# Installation
Once all the parts have been printed, they can be assembled using m2 hexagon socket screws. <br/>
The ESP should be located on the adapter board and all sensors should be connected as follows (please refer to the wiring diagram). <br/>

#GPS#

TX = RX2 GPIO34       //GPS-RX2 <br/>
RX = TX2 GPIO35       //GPS-TX2 <br/>
GND <br/>
3.3V <br/>

#SDS#

RX = Serial2	  GPIO017   //SDS-RX1 <br/>
TX = Serial2   	GPIO016   //SDS-TX1 <br/>
GND <br/>
5V <br/>

#ADS1115# !I2C!

SDA = GPIO 21 <br/>
SCL = GPIO 22 <br/>
GND <br/>
5V <br/>

BME680 !I2C!

SDA = GPIO 21 <br/>
SCL = GPIO 22 <br/>
GND <br/>
3.3V <br/>

#DF-Robot-Micro#

Analog = ADS1115 A0 <br/>
GND <br/>
5V <br/>

#Shake Sensor#

PIN 25 <br/>
GND  <br/>
3,3V <br/>

Display !I2C!
SDA = GPIO 21 <br/>
SCL = GPIO 22 <br/>
GND <br/>
3,3V <br/>

The first Scetch (Webconfig.ino) can now be compiled and uploaded to the ESP, pay attention to the correct WLAN data on line 29 and 30. <br/>
If the ESP restart is completed, the IP (find out via your router) of the ESP can be called up. <br/>
It may also work to simply put the link in your browser http://Station.local <br/>

Now the basic data for communication with your InfluxDB can be stored:

- Name of the sensor
- Destination address of the InfluxDB
- InfluxDB token
- InfluxDB organization
- InfluxDB Bucket

Once the entered data has been accepted, the main firmware (Station-V1.x) can be uploaded.

After this, the sensor is restarting and is now connected to your WiFi.  <br/>
If you want to disconnect the sensor of your wifi, to connect to another WiFi, go to the webpage over the ip address, or http://station_< your defined number in the webconfig >.local  <br/>
Now the should restart and starts a hotspot. You can now follow the steps on the display. <br/>
(Or simply connect a Mobile device to the hotspot and follow the steps on the phone) <br/>

# Used Parts

- ESP32‑IO Development Expansion Board
- ESP32 NodeMCU (Check the correct PIN-Number)
- GPS GY-NEO6MV2
- DfRobot Sound Level Meter
- Analog-Digital-Converter ADS1115
- Temp/Humidity BME680
- Dust-Sensor SDS011 with little pipe of ~3cm (8mm) <br/>
  INFO: On the original cable, the well-known JST 2.54 connectors were attached to the side of the esp
- Cylinder head screws Hexagon socket DIN 912 2&4mm
- AZDelivery 0.96 Zoll OLED Display
- AZDelivery SW420 Vibration Sensor
- Jumper Cables

# Pictures

Fully assembled sensor <br/>
![alt text](https://github.com/doubery/Environmental-Measuring-Box/blob/main/images/Seite_Ausschnitt.JPG)

Webconfig <br/>
![alt text](https://github.com/doubery/Environmental-Measuring-Box/blob/Initial-configuration/images/Webconfig.png)

Settings page after uploaded firmware "station-V1.7.bin" <br/>
![alt text](https://github.com/doubery/Environmental-Measuring-Box/blob/Initial-configuration/images/Weboberflaeche.png)





