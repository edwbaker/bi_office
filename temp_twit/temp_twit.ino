#if defined(ARDUINO) && ARDUINO > 18   // Arduino 0019 or later
#include <SPI.h>
#endif
#include <Ethernet.h>
#include <string.h>
//#include <EthernetDNS.h> string.h arduino
//Only needed in Arduino 0022 or earlier
#include <Twitter.h>
//#include <WString.h>

//When debuggin waiting for DHCP to fail is slow
boolean attempt_dhcp = true;
boolean tweet = true;



//Configure Ethernet connection
byte mac[] = { 
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
IPAddress ip(192,168,1, 177);
IPAddress gateway(192,168,1, 1);
IPAddress subnet(255, 255, 0, 0);

EthernetServer server(80);


//Configure Twitter
Twitter twitter("1096018604-Oy6Lq1bJFp3C6557AbLrmHR9aM2Q3Yph9hvKGU8");
//A message to pass to Twitter
String message;
String page;

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

//Ed phone
unsigned long phone_light_first_on = 0;
unsigned long phone_light_last_on = 0;
unsigned long phone_off_since = 0;
boolean phone_has_message = false;

void setup()
{
  message.reserve(140);
  page.reserve(30);
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
  Serial.println(F("Trying to get an IP address using DHCP"));
  if (attempt_dhcp){
    if (Ethernet.begin(mac) == 0) {
      Serial.println(F("Failed to configure Ethernet using DHCP"));
      Serial.println(F("Attempting manual Ethernet configuration."));
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
  Serial.print(F("server is at "));
  Serial.println(Ethernet.localIP());


  Serial.println(F("connecting ..."));

  twitter_send();

  tempC = (analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin)+analogRead(tempPin))/5;
  tempC = (5.0 * tempC * 100.0)/1024.0;

  send_twitter_temperature();

  light = analogRead(lightPin);

  send_lighting_value();

  Serial.println(F("Starting loop!"));
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

  Serial.println(analogRead(1));

  if (analogRead(1) > 700) {
    phone_light_last_on = millis();
    if (!phone_has_message) {
      if (phone_light_first_on == 0) {
        phone_light_first_on = millis();
        message = "Ed's phone is ringing";
        twitter_send();
        delay(1000);
        message = "D edwbaker You're phone is ringing!";
        twitter_send();
      }
      if (phone_light_first_on + 180000 < phone_light_last_on) {
        phone_has_message = true;
        message = "Ed's phone has an answerphone message.";
        twitter_send();
        message = "D edwbaker You have an answerphone message.";
        twitter_send();
      }
    }
  }

  if (phone_has_message) {
    if (phone_light_last_on + 15000 < millis()) {
      phone_has_message = false;
    } 
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
  message = "";
  EthernetClient client = server.available();
  if (client) {
    Serial.println(F("new HTTP request arrived!"));
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        message.concat(c);
        Serial.write(c);
        //Use message to save RAM
        if (c == '\n') {

          if (message.startsWith("GET ")) {

            message = message.substring(5, message.lastIndexOf(' '));
            page = message;

          }
        }
        if (c == '\n' && currentLineIsBlank) {
          client.println(F("HTTP/1.1 200 OK"));
          client.println(F("Content-Type: text/html"));
          client.println(F("Connnection: close"));
          client.println();
          client.println(F("<!DOCTYPE HTML>"));
          client.println(F("<html>"));

          //TODO: coffee/username
          if (page == "coffee_on") {
            Serial.println(F("Coffee on."));
            client.println(F("<h1>The coffee machine isn't online yet</h1>"));
            client.println(F("<h2>But this would turn it on."));
          }
          else if (page == "coffee_off") {
            client.println(F("<h1>The coffee machine isn't online yet</h1>"));
            client.println(F("<h2>But this would turn it on."));
          }
          else {

            client.println(F("<meta http-equiv=\"refresh\" content=\"5\">"));


            client.println(F("<h1>Office information for you happy people!</h1>"));

            client.print(F("<h2>Enviromental Information</h2>"));
            client.print(F("<b>Current temperature is "));
            client.print(tempC);
            client.print(F("C.</b><br />"));

            client.print(F("Minimum was "));
            client.print(temp_min);
            client.print(F("<br/>"));

            client.print(F("Maximum was "));
            client.print(temp_max);
            client.print(F("<br/>"));

            client.print(F("The lights are "));
            if (light_on == true) {
              client.print(F("on"));
            }
            else {
              client.print(F("off"));
            }
            client.print(F(" ("));
            client.print(light);
            client.println(F(")<br />"));

            client.print(F("<h2>State of telecommunications</h2>"));
            if (phone_has_message) {
              client.print(F("Ed's phone has an answerphone message."));
            }
            else {
              client.print(F("Ed's phone has no messages."));
            }
            client.println(F("<br />"));

          }
          client.println(F("</html>"));
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
    Serial.println(F("client disonnected"));
  }
}

void check_the_lights() {
  if (light_on == true) {
    if (light < light_trigger_off ) {
      light_on = false;
      Serial.println(F("The lights are now off."));
      message = "The lights are now off.";
      twitter_send();
      message = "D edwbaker The lights are now off.";
      twitter_send();

    }
  }
  if (light_on == false) {
    if (light > light_trigger_on) {
      light_on = true;
      Serial.println(F("The lights are now on."));
      message = "The lights are now on.";
      twitter_send();
      message = "D edwbaker The lights are now on.";
      twitter_send();
    }
  }
  light_triggered = millis();
}

void twitter_send() {
  if (tweet) {
    message.concat(" (");
    message.concat(millis());
    message.concat(")\0");

    char charBuf[140];
    message.toCharArray(charBuf, 140);

    Serial.println(F("Preparing to tweet message:"));
    Serial.println(charBuf);

    if (twitter.post(charBuf)) {
      int status = twitter.wait();
      if (status == 200) {
        Serial.println(F("The tweet was sent!"));
      } 
      else {
        Serial.print(F("Tweet failed : code "));
        Serial.println(status);
      }
    } 
    else {
      Serial.println(F("connection failed."));
    }

    Serial.println();
    delay(1000);
  }
  message = "";
}

void server_turn_on_coffee(){
  message = "If the coffee machine was online, it would now be on.";
  twitter_send();

}







