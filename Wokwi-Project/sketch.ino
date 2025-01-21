
// ESP WT32-ETH01 datasheet:
// https://cdn.shopify.com/s/files/1/0698/0193/5164/files/WT32-ETH01_datasheet_V1.3_-_en.pdf?v=1717218833
// Extra info with potential corrections for above:
// https://community-assets.home-assistant.io/original/4X/1/4/2/1424be77c5e91735a9546fb2507250f137cf3cb1.jpeg


// Make sure to include the SoftwareSerial.h file

////////    Config    ////////
#define DEBUG        // Print debug lines to serial, comment out to disable
// #define EXTRA_DEBUG  // Even more debug printing, currently loop timing

//// MIDI
#define MIDI_RX_PIN      5    // Labelled as RXD (May either be pin 35 or 5 on some versions of the board)
#define MIDI_START_NOTE  21   // This note will line up with DMX channel 1
#define MIDI_END_NOTE    108  // Notes past this note won't be forwarded
#define MIDI_CHANNEL     1    // The MIDI channel to listen to in the range [1, 16]
#define MIDI_ATTENUATION 1024 // The attenuation should be in the range [0, 1024], 1024 giving it the full output range of 0-255

//// Network
#define HOSTNAME "midi-to-sacn"
// #define DHCP  // Comment this line out to use below static IP address settings
#define LOCAL_IP "192.168.0.202"
#define GATEWAY  "192.168.0.1"
#define SUBNET   "0.0.0.0"

//// DMX sACN
#define DMX_UNIVERSE 300
#define SACN_RECV_IP "192.168.0.200"  // The sACM receiver's IP address
// Unique ID of your SACN source. You can generate one from https://uuidgenerator.net
// or on the command line by typing uuidgen
#define SACN_UUID    "9E8D426B-C724-42C7-87C6-65FC523F88D7"
////////    End Config    ////////



#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN

#include <ETH.h>
#include <WiFiUdp.h>

#include <MidiDmxBridge.h>

#include <sACNSource.h>


#ifdef DEBUG
  #define DEBUG_BEGIN(x)   Serial.begin (x)
  #define DEBUG_PRINT(x)   Serial.print (x)
  #define DEBUG_PRINTLN(x) Serial.println (x)
#else
  #define DEBUG_BEGIN(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif


WiFiUDP Udp;
volatile static bool eth_connected = false;

// An sACNSource instance using the Ethernet instance:
sACNSource myController(Udp);

HardwareSerial SerialPort(1);

unsigned long prev_time = 0;


// onEvent is called from a separate FreeRTOS task (thread)!
void onEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      DEBUG_PRINTLN("ETH Started");
      // The hostname must be set after the interface is started, but needs
      // to be set before DHCP, so set it from the event handler thread.
      ETH.setHostname(HOSTNAME);
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      DEBUG_PRINTLN("ETH Connected");
  #ifndef DHCP
      eth_connected = true;  // Since there's no ARDUINO_EVENT_ETH_GOT_IP event with static IP
  #endif
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      DEBUG_PRINTLN("ETH Got IP");
      DEBUG_PRINTLN(ETH);
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      DEBUG_PRINTLN("ETH Lost IP");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      DEBUG_PRINTLN("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      DEBUG_PRINTLN("ETH Stopped");
      eth_connected = false;
      break;
    default: break;
  }
}


// class SerialReaderTest : public mididmxbridge::ISerialReader {
//  public:
//   void begin() override { }

//   int available() override { return Serial.available(); }

//   int read() override { return Serial.read(); }

//   void sleep(uint16_t sleep_ms) override { delay(sleep_ms); }
// } reader;

class SerialReader : public mididmxbridge::ISerialReader {
 public:
  void begin() override { SerialPort.begin(31250, SERIAL_8N1, MIDI_RX_PIN, -1); }

  int available() override { return SerialPort.available(); }

  int read() override { return SerialPort.read(); }

  void sleep(uint16_t sleep_ms) override { delay(sleep_ms); }
} reader;

static void onDmxChange(uint8_t channel, const uint8_t value) {
  if (channel >= MIDI_START_NOTE && channel <= MIDI_END_NOTE) {
    channel -= (MIDI_START_NOTE - 1);
    myController.setChannel(channel, value);
    DEBUG_PRINT("Channel: ");
    DEBUG_PRINT(channel);
    DEBUG_PRINT(", Value: ");
    DEBUG_PRINTLN(value);

    myController.sendPacket(SACN_RECV_IP);
  }
}


static MidiDmxBridge MDXBridge(MIDI_CHANNEL, onDmxChange, reader);


void setup() {
  DEBUG_BEGIN(115200);

  //// Ethernet
  Network.onEvent(onEvent);
  ETH.begin();
#ifndef DHCP
  ETH.config(LOCAL_IP, GATEWAY, SUBNET);
#endif

  //// sACN
  // initialize sACN source:
  myController.begin(HOSTNAME, SACN_UUID, DMX_UNIVERSE);

  // set DMX channel values to 0:
  for (int dmxChannel = 0; dmxChannel < 513; dmxChannel++) {
    myController.setChannel(dmxChannel, 0);
  }
  myController.sendPacket(SACN_RECV_IP);

  //// MIDI
  MDXBridge.begin();
  MDXBridge.setAttenuation(MIDI_ATTENUATION);
}


void loop() {
  // Print time since last loop
#ifdef EXTRA_DEBUG
  unsigned long curr_time = millis();
  DEBUG_PRINT("Time since last loop: ");
  DEBUG_PRINTLN(curr_time - prev_time);
  prev_time = curr_time;
#endif

  MDXBridge.listen();
}
