#define WE 13 // connected to an RC circuit with pulse length of ~112 ns
#define OE 12
#define CE 11

#define CE_B (1 << (CE-2))
#define OE_B (1 << (OE-2))
#define WE_B (1<<(WE-2))

#define IO0 2
#define IO7 9

#define ADDR0 22
#define ADDR14 50

// each clock cycle is ~62.5ns on 16MHz clock
// double that (two nops) is ~125ns
// use _NOP()

#define LOG_PAGESIZE 6
#define PAGESIZE (1<<LOG_PAGESIZE) // 64

byte page[PAGESIZE] __attribute__((__aligned__(64)));
byte *write_ptr = page;
byte *read_ptr = page;

void setup() {
  // put your setup code here, to run once:
  for(int pin = IO0; pin <= IO7; pin++){
    pinMode(pin, OUTPUT);
  }
  for(int pin = ADDR0; pin <= ADDR14; pin += 2){
    pinMode(pin, OUTPUT);
  }
  pinMode(OE, OUTPUT);
  pinMode(WE, OUTPUT);
  pinMode(CE, OUTPUT);
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
  digitalWrite(CE, LOW);
  Serial.begin(9600);
}

void loop() {
  size_t bytes = Serial.available();
  if(bytes > 0) {
    size_t bytes_to_read = min(write_ptr - page, bytes);
    if(bytes_to_read) Serial.readBytes(write_ptr, bytes_to_read);
  }
}
