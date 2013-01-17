#if defined(ARDUINO) && ARDUINO > 18   // Arduino 0019 or later
#include <SPI.h>
#endif
#include <Ethernet.h>
//#include <EthernetDNS.h>  Only needed in Arduino 0022 or earlier
#include <Twitter.h>
boolean attempt_dhcp = true;


byte mac[] = { 
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
IPAddress ip(192,168,1, 177);
IPAddress gateway(192,168,1, 1);
IPAddress subnet(255, 255, 0, 0);

EthernetServer server(80);

double tempC;

double temp_min = 1000;
double temp_max = 0;

unsigned long temp_min_time = millis();
unsigned long temp_max_time = millis();

double light;
int tempPin=4;
int lightPin=5;
unsigned long twitter_sent_temp = millis();

double light_trigger_on  = 300.0;
double light_trigger_off = 200.0;
unsigned long light_triggered = millis();
int light_trigger_buffer = 2000;
boolean light_on;

//Variables associated with average readings
const int num_readings = 10;
int light_readings[num_readings];
int temp_readings[num_readings];
int reading_index = 0;
int light_reading_total;
int temp_reading_total;

Twitter twitter("1096018604-Oy6Lq1bJFp3C6557AbLrmHR9aM2Q3Yph9hvKGU8");

void setup()
{
  //Is the light on?
  light = analogRead(lightPin);
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
  String ip_message = "D edwbaker Hi, my IP address is: ";
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(ip[thisByte], DEC);
    Serial.print(".");
    ip_message.concat(ip[thisByte]);
    ip_message.concat(".");
  }

  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());


  Serial.println("connecting ...");

  twitter_send(ip_message);
  
  tempC = analogRead(tempPin);
  tempC = (5.0 * tempC * 100.0)/1024.0;
  
  send_twitter_temperature();

  Serial.println("Starting loop!");
}

void loop()
{
  //Get data

  tempC = analogRead(tempPin);
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

}

void send_twitter_temperature(){
  char temp[5];
  dtostrf(tempC,4,1,temp);
  temp[4] = NULL;
  String temp_msg = "The current temperature is ";
  temp_msg.concat(temp);
  temp_msg.concat("C");

  twitter_send(temp_msg);
  twitter_sent_temp = millis();
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
      String light_message = "The lights are now off.";
      twitter_send(light_message);
    }
  }
  if (light_on == false) {
    if (light > light_trigger_on) {
      light_on = true;
      Serial.println("The lights are now on.");
      String light_message = "The lights are now on.";
      twitter_send(light_message);
    }
  }
  light_triggered = millis();
}

void twitter_send(String message) {

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
  Serial.println();
}


