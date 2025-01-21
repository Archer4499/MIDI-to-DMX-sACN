// MidiDmxBridge attempts to import the SoftwareSerial library but it doesn't exist for ESPs
// Since we don't actually need it, this is a minimal implementation to make it happy

class SoftwareSerial {
 public:
  SoftwareSerial(const uint8_t rxPin, const uint8_t txPin) {}

  void begin(const uint16_t rxPin)  {}

  int available()  { }

  int read()  { }
};