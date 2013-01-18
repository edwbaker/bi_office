#if defined(ARDUINO) && ARDUINO > 18   // Arduino 0019 or later
#include <SPI.h>
#endif
#include <Ethernet.h>
#include <string.h>
//#include <EthernetDNS.h> string.h arduino
//Only needed in Arduino 0022 or earlier
#include <Twitter.h>

//When debuggin waiting for DHCP to fail is slow
boolean attempt_dhcp = true;

//Configure Ethernet connection
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
IPAddress ip(192,168,1, 177);
IPAddress gateway(192,168,1, 1);
IPAddress subnet(255, 255, 0, 0);

EthernetServer server(80);

//Configure Twitter
Twitter twitter("1096018604-Oy6Lq1bJFp3C6557AbLrmHR9aM2Q3Yph9hvKGU8");
//A message to pass to Twitter
String message;

//For the temperature sensor
int tempPin=4;
double tempC;
double temp_min = 1000;
double temp_max = 0;
unsigned long temp_min_time = millis();
unsigned long temp_max_time = millis();

//For the lighting monitor
double light;
int lightPin=5;
unsigned long twitter_sent_temp = millis();
double light_trigger_on  = 520.0;
double light_trigger_off = 500.0;
unsigned long light_triggered = millis();
int light_trigger_buffer = 2000;
boolean light_on;
unsigned long twitter_sent_light = millis();

//Temp variable for double to String conversion
char temp[5];



void setup()
{
  message.reserve(140);
  //Is the light on?
  light = (analogRead(lightPin) + analogRead(lightPin) + analogRead(lightPin) + analogRead(lightPin) + analogRead(lightPin)) / 5;
  if (light > light_trigger_on) {
    light_on = true;
  }
  else {
    light_on = false;
  }
  light_triggered = millis();


  Serial.begin(9600);
  delay(1000);
  Serial.println("Trying to get an IP address using DHCP");
  if (attempt_dhcp){
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Failed to configure Ethernet using DHCP");
      Serial.println("Attempting manual Ethernet configuration.");
      // initialize the ethernet device not using DHCP:
      Ethernet.begin(mac, ip, gateway, subnet);
    }
  }
  else {
    Ethernet.begin(mac, ip, gateway, subnet);
  }

  // print your local IP address:
  Serial.print("My IP address: ");
  ip = Ethernet.localIP();
  message = "D edwbaker Hi, my IP address is: ";
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(ip[thisByte], DEC);
    Serial.print(".");
    message.concat(ip[thisByte]);
    message.concat(".");
  }

  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());


  Serial.println("connecting ...");

  twitter_send();

  tempC = (analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin))/5;
  tempC = (5.0 * tempC * 100.0)/1024.0;

  send_twitter_temperature();
  
  light = analogRead(lightPin);
  
  send_lighting_value();

  Serial.println("Starting loop!");
}

void loop()
{
  //Get data

  tempC = (analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin))/5;
  tempC = (5.0 * tempC * 100.0)/1024.0;

  if (tempC < temp_min){
    temp_min = tempC;
    temp_min_time = millis();
  }

  if (tempC > temp_max) {
    temp_max = tempC;
    temp_max_time = millis();
  }

  light = analogRead(lightPin);


  //Check for incoming connections to server
  server_check_connections();

  //Twitter message
  if (millis() > twitter_sent_temp + 900000) {
    send_twitter_temperature();
  }
  if (millis() > light_triggered + light_trigger_buffer) {
    check_the_lights();
  }
  if (millis() > twitter_sent_light + 1800000) {
    send_lighting_value();
  }

}

void send_twitter_temperature(){
  dtostrf(tempC,4,1,temp);
  temp[4] = NULL;
  message = "The current temperature is ";
  message.concat(temp);
  message.concat("C");

  twitter_send();
  twitter_sent_temp = millis();
}

void send_lighting_value(){
  dtostrf(light,4,0,temp);
  temp[4] = NULL;
  message = "The ambient lighting measures ";
  message.concat(temp);
  message.concat(" on an arbritary scale.");
  twitter_send();
  twitter_sent_light = millis();
}

void server_check_connections(){
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new HTTP request arrived!");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connnection: close");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // add a meta refresh tag, so the browser pulls again every 5 seconds:
          client.println("<meta http-equiv=\"refresh\" content=\"5\">");


          client.println("<h1>Office information for you happy people!</h1>");

          client.print("<h2>Enviromental Information</h2>");
          client.print("<b>Current temperature is ");
          client.print(tempC);
          client.print("C.</b><br />");

          client.print("Minimum was ");
          client.print(temp_min);
          client.print("<br/>");

          client.print("Maximum was ");
          client.print(temp_max);
          client.print("<br/>");

          client.print("The lights are ");
          if (light_on == true) {
            client.print("on");
          }
          else {
            client.print("off");
          }
          client.print(" (");
          client.print(light);
          client.println(")<br />");

          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(100);
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }
}

void check_the_lights() {
  if (light_on == true) {
    if (light < light_trigger_off ) {
      light_on = false;
      Serial.println("The lights are now off.");
      message = "The lights are now off.";
      twitter_send();
    }
  }
  if (light_on == false) {
    if (light > light_trigger_on) {
      light_on = true;
      Serial.println("The lights are now on.");
      message = "The lights are now on.";
      twitter_send();
    }
  }
  light_triggered = millis();
}

void twitter_send() {
  message.concat(" (");
  message.concat(millis());
  message.concat(")\0");

  char charBuf[140];
  message.toCharArray(charBuf, 140);

  Serial.println("Preparing to tweet message:");
  Serial.println(charBuf);

  if (twitter.post(charBuf)) {
    int status = twitter.wait();
    if (status == 200) {
      Serial.println("The tweet was sent!");
    } 
    else {
      Serial.print("Tweet failed : code ");
      Serial.println(status);
    }
  } 
  else {
    Serial.println("connection failed.");
  }
  message = "";
  Serial.println();
  delay(1000);
}



