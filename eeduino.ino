// PG0-PG2
enum ctrl_pin {
  WE = 41,
  OE = 40,
  CE = 39,
};
#define WE_BIT 0
#define OE_BIT 1
#define CE_BIT 2

#define IO0 49 // PL0-PL7
#define IO7 42

#define ADDR0 22 // 22-29 PA0-PA7
#define ADDR7 29
// 22-29 ADDR0-ADDR7
#define UNUSED 30
// pin 30 unused
#define ADDR8 37
#define ADDR14 31 // 37-31 PC0-PC6
// 37-31 = ADDR8-ADDR14

#define USE_PORT_MANIP true

static_assert(F_CPU == 16000000, "clock speed must be 16mhz!");

inline void set_data(byte data) {
  if(USE_PORT_MANIP) {
    PORTL = data;
  } else {
    for(int pin = IO0; pin >= IO7; pin--) {
      digitalWrite(pin, data & 1);
      data >>= 1;
    }
  }
}

inline byte get_data() {
  if(USE_PORT_MANIP) {
    return PINL;
  } else {
    byte result;
    for(int pin = IO0; pin >= IO7; pin--) {
      result |= digitalRead(pin);
      result <<= 1;
    }
    return result;
  }
}

inline void set_addr(unsigned addr) {
  if(USE_PORT_MANIP) {
    PORTA = addr & 0xff;
    PORTC = addr >> 8;
  } else {
    for(int pin = ADDR0; pin <= ADDR7; pin++) {
      digitalWrite(pin, addr & 1);
      addr >>= 1;
    }
    // consistent behavior
    for(int pin = ADDR8; pin >= /* ADDR14 */ UNUSED; pin--) {
      digitalWrite(pin, addr & 1);
      addr >>= 1;
    }
  }
}

template<bool mode>
inline void io_data() {
  if(USE_PORT_MANIP) {
    if(mode == OUTPUT) {
      DDRL = B11111111;
    } else {
      DDRL = B00000000;
    }
  } else {
    for(int pin = IO0; pin >= IO7; pin--) {
      pinMode(pin, mode);
    }
  }
}

// the address is never anything but output
inline void io_out_addr() {
  if(USE_PORT_MANIP) {
    DDRA = B11111111;
    DDRC |= B01111111;
  } else {
    for(int pin = ADDR0; pin <= ADDR7; pin++) {
      pinMode(pin, OUTPUT);
    }
    for(int pin = ADDR8; pin >= ADDR14; pin--) {
      pinMode(pin, OUTPUT);
    }
  }
}

// same with control lines
inline void io_out_ctrl() {
  if(USE_PORT_MANIP) {
    DDRG |= B00000111;
  } else {
    pinMode(CE, OUTPUT);
    pinMode(OE, OUTPUT);
    pinMode(WE, OUTPUT);
  }
}

template<enum ctrl_pin pin, bool level>
inline void write_ctrl() {
  static_assert(pin == CE || pin == OE || pin == WE,
                "invalid control pin");
  if(USE_PORT_MANIP) {
    int n = -(pin - WE);
    if(level) {
      PORTG |= (1 << n);
    } else {
      PORTG &= ~(1 << n);
    }
  } else {
    digitalWrite(pin, level);
  }
}

void writeByte(byte data, unsigned address) {
  set_data(data);
  set_addr(address);
  write_ctrl<CE, LOW>();
  write_ctrl<WE, LOW>(); // active low
  _NOP(); _NOP(); // wait 125 ns, write pulse low width
  // probably not needed
  write_ctrl<WE, HIGH>();
  write_ctrl<CE, HIGH>();
  _NOP(); // wait 62.5 ns, write pulse high width
}

byte readByte(unsigned address) {
  set_addr(address);
  write_ctrl<CE, LOW>();
  write_ctrl<OE, LOW>();
  _NOP(); _NOP(); _NOP(); // wait 187.5ns, address to output delay
  byte value = get_data();
  write_ctrl<OE, HIGH>();
  write_ctrl<CE, HIGH>();
  return value;
}

void disableSDP() {
  noInterrupts();
  writeByte(0xaa, 0x5555);
  writeByte(0x55, 0x2aaa);
  writeByte(0x80, 0x5555);
  writeByte(0xaa, 0x5555);
  writeByte(0x55, 0x2aaa);
  writeByte(0x20, 0x5555);
  writeByte(0x69, 0x0000); // dummy write to flush command
  delay(10);
  interrupts();
}

void erase() {
  noInterrupts();
  writeByte(0xaa, 0x5555);
  writeByte(0x55, 0x2aaa);
  writeByte(0x80, 0x5555);
  writeByte(0xaa, 0x5555);
  writeByte(0x55, 0x2aaa);
  writeByte(0x10, 0x5555);
  delay(20);
  interrupts();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  write_ctrl<CE, HIGH>();
  write_ctrl<OE, HIGH>();
  write_ctrl<WE, HIGH>();

  io_out_addr();
  io_out_ctrl();

  delay(5); // power-on delay
if(0) {
  io_data<OUTPUT>();
  disableSDP();
}
if(0) {
  io_data<OUTPUT>();
  erase();
}

  unsigned char byte_to_write = 0xff;
  unsigned address = 0x0000;
if(0) {
  io_data<OUTPUT>();
  // write
  noInterrupts();
  writeByte(byte_to_write, address);
}

if(0) {
  delay(10); // wait 10 ms for write cycle to complete
  interrupts();
} else if(0) {
  // /DATA poll

  io_data<INPUT>();
  set_data(0); // clear pullups

  while((readByte(address) ^ byte_to_write) & 0x80) {}
  interrupts();
}

if(1) {
  // read
  io_data<INPUT>();
  set_data(0); // clear pullups
  for(int addr = 1; addr < 2; addr++) {
    noInterrupts();
    byte value = readByte(addr);
    interrupts();
    Serial.println(value, HEX);
  }
}
  Serial.println(F("Done!"));
}

void loop() {
}
