#include "emonesp.h"
#include "input.h"
#include "config.h"

const char *e_url = "/input/post.json?node=";

String url = "";
String data = "";

int espflash = 0;
int espfree = 0;

int commDelay = 60;
int rapi_command = 1;
int rapi_command_sent = 0;
int comm_Delay = 1000;          //Delay between each command and read or write
unsigned long comm_Timer = 0;   //Timer for Comm requests

String amp = "-";                    //OpenEVSE Current Sensor
String volt = "-";                   //Not currently in used
String temp1 = "-";                  //Sensor DS3232 Ambient
String temp2 = "-";                  //Sensor MCP9808 Ambient
String temp3 = "-";                  //Sensor TMP007 Infared
String pilot = "-";                  //OpenEVSE Pilot Setting
long state = 0;                 //OpenEVSE State
String estate = "Unknown";      // Common name for State

//Defaults OpenEVSE Settings
byte rgb_lcd = 1;
byte serial_dbg = 0;
byte auto_service = 1;
int service = 1;
int current_l1 = 0;
int current_l2 = 0;
String current_l1min = "-";
String current_l2min = "-";
String current_l1max = "-";
String current_l2max = "-";
String current_scale = "-";
String current_offset = "-";
//Default OpenEVSE Safety Configuration
byte diode_ck = 1;
byte gfci_test = 1;
byte ground_ck = 1;
byte stuck_relay = 1;
byte vent_ck = 1;
byte temp_ck = 1;
byte auto_start = 1;
String firmware = "-";
String protocol = "-";

//Default OpenEVSE Fault Counters
String gfci_count = "-";
String nognd_count = "-";
String stuck_count = "-";
//OpenEVSE Session options
String kwh_limit = "0";
String time_limit = "0";

//OpenEVSE Usage Statistics
String wattsec = "0";
String watthour_total = "0";

unsigned long comm_sent = 0;
unsigned long comm_success = 0;


void
create_rapi_json() {
  url = e_url;
  data = "";
  url += String(emoncms_node) + "&json={";
  data += "amp:" + amp + ",";
  data += "temp1:" + temp1 + ",";
  data += "temp2:" + temp2 + ",";
  data += "temp3:" + temp3 + ",";
  data += "pilot:" + pilot + ",";
  data += "state:" + String(state);
  url += data;
  if (emoncms_server == "data.openevse.com/emoncms") {
    // data.openevse uses device module
    url += "}&devicekey=" + String(emoncms_apikey);
  } else {
    // emoncms.org does not use device module
    url += "}&apikey=" + String(emoncms_apikey);
  }

  //DEBUG.print(emoncms_server.c_str() + String(url));
}

// Send RAPI command & increment stats
void rapi_send(String cmd)
{
  Serial.println(cmd);
  comm_sent++;
}

String unused;

// Parse RAPI response into val1, val2, val3
// as appropriate.  Pass string 'unused' for vals to ignore
// increment stats on success
void rapi_parse(String& val1, String& val2, String& val3)
{
  while (Serial.available()) {
    String rapiResponse = Serial.readStringUntil('\r');
    if (rapiResponse.startsWith("$OK")) {
      comm_success++;

      // Strip out checksum char & checksum val for now (check later)
      int checkSumChar = rapiResponse.indexOf('^');
      if (checkSumChar > 0) {
        rapiResponse.remove(checkSumChar);
      }

      int firstRapiCmd = 0, secondRapiCmd = 0, thirdRapiCmd = 0;

      firstRapiCmd = rapiResponse.indexOf(' ');

      if (firstRapiCmd > 0) {
        secondRapiCmd = rapiResponse.indexOf(' ', firstRapiCmd + 1); // pos or -1
        val1 = rapiResponse.substring(firstRapiCmd + 1, secondRapiCmd);
      }

      if (secondRapiCmd > 0) {
        thirdRapiCmd = rapiResponse.indexOf(' ', secondRapiCmd + 1); // pos or -1
        val2 = rapiResponse.substring(secondRapiCmd + 1, thirdRapiCmd);
      }

      if (thirdRapiCmd > 0)
        val3 = rapiResponse.substring(thirdRapiCmd + 1);
    }
  }
}

// -------------------------------------------------------------------
// OpenEVSE Request
//
// Get RAPI Values
// Runs from arduino main loop, runs a new command in the loop
// on each call.  Used for values that change at runtime.
// Complex state is to avoid waiting for serial response and bogging
// down the UI.
// -------------------------------------------------------------------

void
update_rapi_values() {
  if ((millis() - comm_Timer) >= comm_Delay) {
    if (rapi_command_sent == 0) {
      Serial.flush(); // wait for output buffer
    }

    if (rapi_command_sent == 0 && rapi_command == 1) {
      espfree = ESP.getFreeHeap();
      rapi_send("$GE*B0");
    }
    if (rapi_command_sent == 1 && rapi_command == 1) {
      rapi_parse(pilot, unused, unused);
    }
    if (rapi_command_sent == 0 && rapi_command == 2) {
      rapi_send("$GS*BE");
    }
    if (rapi_command_sent == 1 && rapi_command == 2) {
      String state_s;
      rapi_parse(state_s, unused, unused);
      state = strtol(state_s.c_str(), NULL, 16);
      if (state == 1)   estate = "Not_Connected";
      if (state == 2)   estate = "EV_Connected";
      if (state == 3)   estate = "Charging";
      if (state == 4)   estate = "Vent_Required";
      if (state == 5)   estate = "Diode_Check_Failed";
      if (state == 6)   estate = "GFCI_Fault";
      if (state == 7)   estate = "No_Earth_Ground";
      if (state == 8)   estate = "Stuck_Relay";
      if (state == 9)   estate = "GFCI_Self_Test_Failed";
      if (state == 10)  estate = "Over_Temperature";
      if (state == 254) estate = "Sleeping";
      if (state == 255) estate = "Disabled";
    }
    if (rapi_command_sent == 0 && rapi_command == 3) {
      rapi_send("$GG*B2");
    }
    if (rapi_command_sent == 1 && rapi_command == 3) {
      rapi_parse(amp, volt, unused);
    }
    if (rapi_command_sent == 0 && rapi_command == 4) {
      rapi_send("$GP*BB");
    }
    if (rapi_command_sent == 1 && rapi_command == 4) {
      rapi_parse(temp1, temp2, temp3);
    }
    if (rapi_command_sent == 0 && rapi_command == 5) {
      rapi_send("$GU*C0");
    }
    if (rapi_command_sent == 1 && rapi_command == 5) {
      rapi_parse(wattsec, watthour_total, unused);
    }
    if (rapi_command_sent == 0 && rapi_command == 6) {
      rapi_send("$GF*B1");
    }
    if (rapi_command_sent == 1 && rapi_command == 6) {
      rapi_parse(gfci_count, nognd_count, stuck_count);
      rapi_command = 0;         //Last RAPI command
    }
    if (rapi_command_sent == 0) {
      rapi_command_sent = 1;
    } else {
      rapi_command++;
      rapi_command_sent = 0;
    }
    comm_Timer = millis();
  }
}

void
handleRapiRead() {
  Serial.flush(); // Flush output buffer
  rapi_send("$GV*C1");
  delay(commDelay);
  rapi_parse(firmware, protocol, unused);
  rapi_send("$GA*AC");
  delay(commDelay);
  rapi_parse(current_scale, current_offset, unused);
  rapi_send("$GH");
  delay(commDelay);
  rapi_parse(kwh_limit, unused, unused);
  rapi_send("$G3");
  delay(commDelay);
  rapi_parse(time_limit, unused, unused);
  rapi_send("$GE*B0");
  delay(commDelay);
  String flag;
  rapi_parse(pilot, flag, unused);
  long flags = strtol(flag.c_str(), NULL, 16);
  service = bitRead(flags, 0) + 1;
  diode_ck = bitRead(flags, 1);
  vent_ck = bitRead(flags, 2);
  ground_ck = bitRead(flags, 3);
  stuck_relay = bitRead(flags, 4);
  auto_service = bitRead(flags, 5);
  auto_start = bitRead(flags, 6);
  serial_dbg = bitRead(flags, 7);
  rgb_lcd = bitRead(flags, 8);
  gfci_test = bitRead(flags, 9);
  temp_ck = bitRead(flags, 10);

  rapi_send("$GC*AE");
  delay(commDelay);
  String current_min, current_max;
  rapi_parse(current_min, current_max, unused);
  if (service == 1) {
    current_l1min = current_min;
    current_l1max = current_max;
  } else {
    current_l2min = current_min;
    current_l2max = current_max;
  }
}
