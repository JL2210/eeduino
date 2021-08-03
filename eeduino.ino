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

void write_byte(byte data, unsigned address) {
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

byte read_byte(unsigned address) {
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
  while((read_byte(last_address) ^ last_byte) & 0x80);
}

void disable_SDP() {
  io_data<INPUT>();
  noInterrupts();
  byte old = read_byte(0x0000);
  io_data<OUTPUT>();
  write_byte(0xaa, 0x5555);
  write_byte(0x55, 0x2aaa);
  write_byte(0x80, 0x5555);
  write_byte(0xaa, 0x5555);
  write_byte(0x55, 0x2aaa);
  write_byte(0x20, 0x5555);
  write_byte(old, 0x0000); // dummy write to flush command
  interrupts();
  delay(10);
}

void enable_SDP() {
  io_data<INPUT>();
  noInterrupts();
  byte old = read_byte(0x0000);
  io_data<OUTPUT>();
  write_byte(0xaa, 0x5555);
  write_byte(0x55, 0x2aaa);
  write_byte(0xa0, 0x5555);
  write_byte(old, 0x0000); // dummy write to flush command
  interrupts();
  delay(10);
}

void erase() {
  io_data<OUTPUT>();
  noInterrupts();
  write_byte(0xaa, 0x5555);
  write_byte(0x55, 0x2aaa);
  write_byte(0x80, 0x5555);
  write_byte(0xaa, 0x5555);
  write_byte(0x55, 0x2aaa);
  write_byte(0x10, 0x5555);
  interrupts();
  delay(20);
}

#define PAGESIZE 64
byte pagebuf[PAGESIZE] = {};
#define NPAGES 512
#define EEPROM_SIZE PAGESIZE * NPAGES

// reads pages [0,n] from the eeprom
void eeprom_readout(unsigned pages) {
  io_data<INPUT>();
  for(unsigned i = 0; i <= pages; i++) {
    noInterrupts();
    for(unsigned n = 0; n < PAGESIZE; n++) {
      pagebuf[n] = read_byte((i * PAGESIZE) + n);
    }
    interrupts();
    Serial.write(pagebuf, PAGESIZE);
    Serial.flush();
  }
}

// writes pages [0,n] to the eeprom
void eeprom_write(unsigned pages) {
  for(unsigned i = 0; i <= pages; i++) {
    while(Serial.available() < PAGESIZE) {
    }
    Serial.readBytes(pagebuf, PAGESIZE);
    noInterrupts();
    unsigned n;
    io_data<OUTPUT>();
    for(n = 0; n < PAGESIZE; n++) {
      write_byte(pagebuf[n], (i * PAGESIZE) + n);
    }
    n--;
    io_data<INPUT>();
    data_poll((i * PAGESIZE) + n, pagebuf[n]);
    interrupts();
  }
}

void setup() {
  // put your setup code here, to run once:
  write_ctrl<CE, HIGH>();
  write_ctrl<OE, HIGH>();
  write_ctrl<WE, HIGH>();

  io_out_addr();
  io_out_ctrl();
  delay(5); // power-on delay

  Serial.begin(115200);
}

void loop() {
  while(Serial.available() < 1) {
  }

  byte command = Serial.read();
  switch(command) {
    case 'e': {
      erase();
      break;
    }
    case 's': {
      disable_SDP();
      break;
    }
    case 'S': {
      enable_SDP();
      break;
    }
    case 'r': case 'w': {
      while(Serial.available() < 2) {
      }
      unsigned pages = Serial.read() & ((NPAGES - 1) & 0xff);
      pages |= (Serial.read() & ((NPAGES - 1) >> 8)) << 8;
      if(command == 'r') {
        eeprom_readout(pages);
      } else if(command == 'w') {
        eeprom_write(pages);
      }
      break;
    }
    case 'd': { // dump whole eeprom
      eeprom_readout(NPAGES - 1);
      break;
    }
    case 'f': { // flash whole EEPROM
      eeprom_write(NPAGES - 1);
      break;
    }
    case 0xee: { // polling byte, echo back
      Serial.write(0xee);
      Serial.flush();
      break;
    }
    default: {
      while(true) {
        pinMode(13, OUTPUT);
        digitalWrite(13, HIGH);
        delay(1000);
        digitalWrite(13, LOW);
        delay(100);
      }
      break;
    }
  }
}
