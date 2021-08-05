// 41-39 PG0-PG2
#define WE 0 // 41
#define OE 1 // 40
#define CE 2 // 39

// 49-42 PL0-PL7
// 49-42 IO0-IO7

// 22-29 PA0-PA7
// 22-29 ADDR0-ADDR7
// pin 30 unused
// 37-31 PC0-PC6
// 37-31 ADDR8-ADDR14

static_assert(F_CPU == 16000000, "clock speed must be 16mhz!");

inline void set_data(byte data) {
  PORTL = data;
}

inline byte get_data() {
  return PINL;
}

inline void set_addr(unsigned addr) {
  PORTA = addr & 0xff;
  PORTC = addr >> 8;
}

template<bool mode>
inline void io_data() {
  if(mode == OUTPUT) {
    DDRL = B11111111;
  } else if(mode == INPUT) {
    DDRL = B00000000;
  }
}

// the address is never anything but output
inline void io_out_addr() {
  DDRA = B11111111;
  DDRC |= B01111111;
}

// same with control lines
inline void io_out_ctrl() {
  DDRG |= B00000111;
}

template<int pin, bool level>
inline void write_ctrl() {
  static_assert(pin == CE || pin == OE || pin == WE,
                "invalid control pin");
  if(level) {
    PORTG |= (1 << pin);
  } else {
    PORTG &= ~(1 << pin);
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
  //_NOP(); // wait 62.5 ns, write pulse high width
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
  while((read_byte(last_address) ^ last_byte) & 0x80) {}
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

// reads all pages the eeprom
void eeprom_dump() {
  io_data<INPUT>();
  for(unsigned page = 0; page < NPAGES; page++) {
    for(unsigned n = 0; n < PAGESIZE; n++) {
      pagebuf[n] = read_byte((page * PAGESIZE) + n);
    }
    Serial.write(pagebuf, PAGESIZE);
    Serial.flush();
  }
}

// writes all pages to the eeprom
void eeprom_flash() {
  for(unsigned page = 0; page < NPAGES; page++) {
    Serial.write(0xaa);
    Serial.flush();
    int nread = Serial.readBytes(pagebuf, PAGESIZE);
    if(nread == 0) {
      pinMode(13, OUTPUT);
      while(true){digitalWrite(13, HIGH);delay(100);digitalWrite(13,LOW);delay(100);}
    }
    noInterrupts();
    io_data<OUTPUT>();
    for(unsigned n = 0; n < PAGESIZE; n++) {
      write_byte(pagebuf[n], (page * PAGESIZE) + n);
    }
    interrupts();
    unsigned last = PAGESIZE - 1;
    io_data<INPUT>();
    data_poll((page * PAGESIZE) + last, pagebuf[last]);
  }
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
  Serial.write(0xab);
  Serial.flush();
}

void loop() {
  while(Serial.available() < 1) {
  }

  byte command = Serial.read();
  switch(command) {
    case 'e':
      erase();
      break;
    case 's':
      disable_SDP();
      break;
    case 'S':
      enable_SDP();
      break;
    case 'd': // dump whole eeprom
      eeprom_dump();
      break;
    case 'f': // flash whole EEPROM
      eeprom_flash();
      break;
    case 0xee: // polling byte, echo back
      Serial.write(0xee);
      Serial.flush();
      break;
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
