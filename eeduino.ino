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

inline void data_poll(unsigned last_address, byte last_byte) {
  while((readByte(last_address) ^ last_byte) & 0x80);
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
  Serial.begin(115200);

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

  randomSeed(0x73);

  byte rdata[64];
  for(int i = 0; i < 64; i++) {
    rdata[i] = random(0x100);
  }

#if 1
  unsigned long start_time = micros();

  io_data<OUTPUT>();
  // write
  noInterrupts();
  unsigned write_addr;
  for(write_addr = 0; write_addr < 64; write_addr++) {
    writeByte(rdata[write_addr], write_addr);
  }
  write_addr--;
#endif

  // /DATA poll
#if 0
  io_data<INPUT>();
  set_data(0); // clear pullups

  data_poll(write_addr, rdata[write_addr]);
  interrupts();
  unsigned long time_taken = micros() - start_time;
#else
  int time_taken = 0;
#endif

#if 1
  // read
  io_data<INPUT>();
  set_data(0); // clear pullups
  bool error = false;
  for(int read_addr = 0; read_addr < 64; read_addr++) {
    noInterrupts();
    byte read_value = readByte(read_addr);
    interrupts();
    byte written_value = rdata[read_addr];
    Serial.print(F("value: "));
    Serial.println(written_value, HEX);
    if(read_value != written_value) {
      error = true;
      Serial.print(F("ERR! "));
      Serial.println(read_value, HEX);
    }
  }
  Serial.print(F("time taken: "));
  Serial.print(time_taken, DEC);
  Serial.println(F(" microseconds"));
  Serial.print(F("had error? "));
  Serial.println(error ? F("yes") : F("no"));
#endif
  Serial.println(F("Done!"));
}

void loop() {
}
