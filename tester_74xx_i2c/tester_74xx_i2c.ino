// I2C Slave Address 7bit op code (0100_A2_A1_A0)
#define I2C_S_ADDR 0b0100000

#define SW_IC_SELECT 2
#define SW_TEST_START 3

#define GPIOA 0x12
#define GPIOB 0x13

#include <Wire.h>
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
//LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
LiquidCrystal lcd(4, 5, 6, 7, 8, 9);

#define NO_TYPES 9
char* ic_type[] = {
  "IV1 (74LS04)", // 1-input NOT
  "AN2 (74LS08)", // 2-input AND
  "OR2 (74LS32)", // 2-input OR
  "ND2 (74LS00)", // 2-input NAND
  "ND3 (74LS10)", // 3-input NAND
  "NR2 (74LS02)", // 2-input NOR
  "EO2 (74LS86)", // 2-input XOR
  "DFF (74LS74)", // D flip-flop
  "JKF (74LS107)" // JK flip-flop
};

/*
    74LSxxの各pinへ
    13 12 11 10  9  8                   VDD GND  GND  GND
     |  |  |  |  |  |  |  |   |    |     |   |    |    |
   +-+--+--+--+--+--+--+--+---+----+-----+---+----+----+---+
   | A7 A6 A5 A4 A3 A2 A1 A0 INTA INTB ^RES ADR2 ADR1 ADR0 |
   |                                                       |
   |>                       MCP23017                       |
   |                                                       |
   | B0 B1 B2 B3 B4 B5 B6 B7 VDD  GND   NC  SCL  SDA  NC  .|
   +-+--+--+--+--+--+--+--+---+----+-----+---+----+----+---+
     |  |  |  |  |  |  |  |   |    |     |   |    |    |
     1  2  3  4  5  6        VDD  GND       A5   A4
    74LSxxの各pinへ                      Arduinoの各pinへ

   I2C receive format (16bit)
   Bit   | [15][14][13][12][11][10][9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
  Name   |  A7  A6  A5  A4  A3  A2  A1  A0  B7  B6  B5  B4  B3  B2  B1  B0
  74pin  |  13  12  11  10   9   8                   6   5   4   3   2   1
  ND2_DIR| out out  in out out  in                  in out out  in out out
  ND3_DIR| out  in out out out  in                  in out out out out out
  NR2_DIR|  in out out  in out out                 out out  in out out  in
  IV1_DIR| out  in out  in out  in                  in out  in out  in out
  DFF_DIR| out out out out  in  in                  in  in out out out out
  JKF_DIR| out out out out out out                  in  in out  in  in out
*/
// bit number        5432109876543210
#define ND2_IO_DIR 0b0010011111100100 // I/O Direction 0:output 1:input
#define ND3_IO_DIR 0b0100011111100000 // I/O Direction 0:output 1:input
#define NR2_IO_DIR 0b1001001111001001 // I/O Direction 0:output 1:input
#define IV1_IO_DIR 0b0101011111101010 // I/O Direction 0:output 1:input
#define DFF_IO_DIR 0b0000111111110000 // I/O Direction 0:output 1:input
#define JKF_IO_DIR 0b0000001111110110 // I/O Direction 0:output 1:input

volatile int mode = -1;
volatile bool flag_test = false;

int lcd_mode = -1;
int pt, ct;
char* selected_ic = NULL;

void setup() {
  selected_ic = (char*)malloc(sizeof(char) * 5);
  lcd.begin(16, 2);  // set columns and rows of LCD
  lcd.setCursor(0, 0);
  lcd.print("Tester for 74xx"); // max 16 character
  lcd.setCursor(0, 1);
  lcd.print("    version 1.5 ");

  // set switch direction
  pinMode(SW_IC_SELECT, INPUT_PULLUP);
  pinMode(SW_TEST_START, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SW_IC_SELECT), change_mode, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW_TEST_START), start_test, FALLING);

  // set the slaveSelectPin as an output:
  Wire.begin();
  write_I2C_port(0x0A, 0b00000000); // IOCON – I/O EXPANDER CONFIGURATION REGISTER
  write_I2C_port(0x0C, 0b11111111); // GPPUA – PULL-UP RESISTOR CONFIGURATION REGISTER BANK=0
  write_I2C_port(0x0D, 0b11111111); // GPPUB – PULL-UP RESISTOR CONFIGURATION REGISTER BANK=0
  write_I2C_port(0x00, 0b11111111); // I/O Direction A 0:output 1:input (Default : input)
  write_I2C_port(0x01, 0b11111111); // I/O Direction B 0:output 1:input プルアップしてるから安全かなと

  Serial.begin(9600);
  delay(100);
  flag_test = false;

  write_I2C_port(0x00, 0x00); // I/O Direction A 0:output 1:input (Default : input)
  write_I2C_port(0x01, 0x00); // I/O Direction B 0:output 1:input プルアップしてるから安全かなと
}

void loop() {
  //  Serial.println(flag_test);
  if ( lcd_mode != mode ) {
    lcd_print_mode();
    flag_test = false;
  }

  if ( flag_test == true ) {
    flag_test = false;
    lcd_print_mode();
    delay(500);
    exec_test(); // test execution
    //    delay(1000);
    // テストが終わったら全ピンをinputに変えて安全にしておく
    write_I2C_port(0x00, 0b11111111); // I/O Direction A 0:output 1:input (Default : input)
    write_I2C_port(0x01, 0b11111111); // I/O Direction B 0:output 1:input プルアップしてるから安全かなと
    flag_test = false;
  } else {
    delay(200);
  }
}

void exec_test() {
  unsigned int response = 0x0000; // 0: OK / 1: NG
  Serial.print("Test start -> ");
  //  mode = 6;
  get_ic_name3(mode);
  if ( strcmp(selected_ic, "IV1") == 0 ) {
    Serial.println("NOT");
    response |= apply_to_1input1(0, 1);
    response |= apply_to_1input1(1, 0);
  } else if ( strcmp(selected_ic, "AN2") == 0 ) {
    Serial.println("AND");
    response |= apply_to_2input1(0, 0, 0);
    response |= apply_to_2input1(0, 1, 0);
    response |= apply_to_2input1(1, 0, 0);
    response |= apply_to_2input1(1, 1, 1);
  } else if ( strcmp(selected_ic, "OR2") == 0 ) {
    Serial.println("OR");
    response |= apply_to_2input1(0, 0, 0);
    response |= apply_to_2input1(0, 1, 1);
    response |= apply_to_2input1(1, 0, 1);
    response |= apply_to_2input1(1, 1, 1);
  } else if ( strcmp(selected_ic, "ND2") == 0 ) {
    Serial.println("NAND");
    response |= apply_to_2input1(0, 0, 1);
    response |= apply_to_2input1(0, 1, 1);
    response |= apply_to_2input1(1, 0, 1);
    response |= apply_to_2input1(1, 1, 0);
  } else if ( strcmp(selected_ic, "ND3") == 0 ) {
    Serial.println("NAND3");
    response |= apply_to_3input1(0, 0, 0, 1);
    response |= apply_to_3input1(0, 0, 1, 1);
    response |= apply_to_3input1(0, 1, 0, 1);
    response |= apply_to_3input1(0, 1, 1, 1);
    response |= apply_to_3input1(1, 0, 0, 1);
    response |= apply_to_3input1(1, 0, 1, 1);
    response |= apply_to_3input1(1, 1, 0, 1);
    response |= apply_to_3input1(1, 1, 1, 0);
  } else if ( strcmp(selected_ic, "EO2") == 0 ) {
    Serial.println("XOR");
    response |= apply_to_2input1(0, 0, 0);
    response |= apply_to_2input1(0, 1, 1);
    response |= apply_to_2input1(1, 0, 1);
    response |= apply_to_2input1(1, 1, 0);
  } else if ( strcmp(selected_ic, "NR2") == 0 ) {
    Serial.println("NOR");
    response |= apply_to_2input2(0, 0, 1);
    response |= apply_to_2input2(0, 1, 0);
    response |= apply_to_2input2(1, 0, 0);
    response |= apply_to_2input2(1, 1, 0);
  } else if ( strcmp(selected_ic, "DFF") == 0 ) {
    Serial.println("DFF");   // ck re pr d  q  qn
    response |= apply_to_4input1(0, 0, 1, 0, 0, 1); // reset
    response |= apply_to_4input1(0, 1, 0, 0, 1, 0); // set
    response |= apply_to_4input1(0, 1, 1, 0, 1, 0); // interval
    response |= apply_to_4input1(1, 1, 1, 0, 0, 1); // positive edge triger D=1->0
    response |= apply_to_4input1(0, 1, 1, 0, 0, 1); // interval
    response |= apply_to_4input1(1, 1, 1, 0, 0, 1); // positive edge triger D=0->0
    response |= apply_to_4input1(1, 1, 1, 1, 0, 1); // hold
    response |= apply_to_4input1(0, 1, 1, 1, 0, 1); // interval
    response |= apply_to_4input1(1, 1, 1, 1, 1, 0); // positive edge triger D=0->1
    response |= apply_to_4input1(0, 1, 1, 1, 1, 0); // interval
    response |= apply_to_4input1(1, 1, 1, 1, 1, 0); // positive edge triger D=1->1
    response |= apply_to_4input1(1, 1, 1, 0, 1, 0); // hold
    response |= apply_to_4input1(1, 0, 1, 0, 0, 1); // reset
  } else if ( strcmp(selected_ic, "JKF") == 0 ) {
    Serial.println("JKF");   // ck re j  k  q  qn
    response |= apply_to_4input2(1, 0, 1, 0, 0, 1); // reset
    response |= apply_to_4input2(1, 1, 0, 0, 0, 1); // J=K=0 Q=0 Preserve
    response |= apply_to_4input2(1, 1, 1, 0, 0, 1); // J=1 K=0 Q=0->1
    response |= apply_to_4input2(0, 1, 1, 0, 1, 0); // negative edge triger Q=0->1
    response |= apply_to_4input2(1, 1, 0, 0, 1, 0); // J=K=0 Q=1 Preserve
    response |= apply_to_4input2(0, 1, 0, 0, 1, 0); // negative edge triger Q=0->0
    response |= apply_to_4input2(1, 1, 1, 1, 1, 0); // J=K=1 Toggle Q=1->0
    response |= apply_to_4input2(0, 1, 1, 1, 0, 1); // negative edge triger Q=1->0
    response |= apply_to_4input2(1, 1, 1, 1, 0, 1); // J=K=1 Toggle Q=0->1
    response |= apply_to_4input2(0, 1, 1, 1, 1, 0); // negative edge triger Q=1->0
    response |= apply_to_4input2(1, 1, 0, 1, 1, 0); // J=0 K=1 Q=1->0
    response |= apply_to_4input2(0, 1, 0, 1, 0, 1); // negative edge triger Q=1->0
    response |= apply_to_4input2(1, 0, 0, 0, 0, 1); // reset
  } else {
    response = 0xffff;
  }
  Serial.println("end!");
  if ( response == 0 ) {
    lcd_print_result("OK", response);
  } else {
    lcd_print_result("NG", response);
  }
  Serial.println("");
}

// ND2, AN2, OR2, EO2
unsigned int apply_to_2input1( byte test_a, byte test_b, byte expect ) {
  unsigned int a = (test_a << 15) | (test_a << 12) | (test_a << 4) | (test_a << 1); // 13pin 10pin 5pin 2pin (74LS00)
  unsigned int b = (test_b << 14) | (test_b << 11) | (test_b << 3) | (test_b << 0); // 12pin  9pin 4pin 1pin (74LS00)
  unsigned int e = (expect << 13) | (expect << 10) | (expect << 5) | (expect << 2); // 11pin  8pin 6pin 3pin (74LS00)
  return apply_test( (~ND2_IO_DIR & (a | b)), e, ND2_IO_DIR );
}
// ND3
unsigned int apply_to_3input1( byte test_a, byte test_b, byte test_c, byte expect ) {
  unsigned int a = (test_a << 12) | (test_a <<  2) | (test_a << 0); // 10pin 3pin 1pin (74LS10)
  unsigned int b = (test_b << 13) | (test_b <<  3) | (test_b << 1); // 11pin 4pin 2pin (74LS10)
  unsigned int c = (test_c << 15) | (test_c << 11) | (test_c << 4); // 13pin 9pin 5pin (74LS10)
  unsigned int e = (expect << 14) | (expect << 10) | (expect << 5); // 12pin 8pin 6pin (74LS10)
  return apply_test( (~ND3_IO_DIR & (a | b | c)), e, ND3_IO_DIR );
}
// NR2
unsigned int apply_to_2input2( byte test_a, byte test_b, byte expect ) {
  unsigned int a = (test_a << 13) | (test_a << 10) | (test_a << 5) | (test_a << 2); // 11pin  8pin 6pin 3pin (74LS02)
  unsigned int b = (test_b << 14) | (test_b << 11) | (test_b << 4) | (test_b << 1); // 12pin  9pin 5pin 2pin (74LS02)
  unsigned int e = (expect << 15) | (expect << 12) | (expect << 3) | (expect << 0); // 13pin 10pin 4pin 1pin (74LS02)
  return apply_test( (~NR2_IO_DIR & (a | b)), e, NR2_IO_DIR );
}
// IV1
unsigned int apply_to_1input1( byte test_a, byte expect ) {
  unsigned int a = (test_a << 15) | (test_a << 13) | (test_a << 11) | (test_a << 4) | (test_a << 2) | (test_a << 0); // 13pin 11pin 9pin 5pin 3pin 1pin (74LS04)
  unsigned int e = (expect << 14) | (expect << 12) | (expect << 10) | (expect << 5) | (expect << 3) | (expect << 1); // 12pin 10pin 8pin 6pin 4pin 2pin (74LS04)
  return apply_test( (~IV1_IO_DIR & a), e, IV1_IO_DIR );
}
// DFF (74LS74)
unsigned int apply_to_4input1( byte test_clk, byte test_clr, byte test_pr, byte test_d, byte exp_q, byte exp_qn ) {
  unsigned int a = (test_clk << 13) | (test_clk << 2); // 11pin 3pin (74LS74)
  unsigned int b = (test_clr << 15) | (test_clr << 0); // 13pin 1pin (74LS74)
  unsigned int c = (test_pr  << 12) | (test_pr  << 3); // 10pin 4pin (74LS74)
  unsigned int d = (test_d   << 14) | (test_d   << 1); // 12pin 2pin (74LS74)
  unsigned int e = (exp_q    << 11) | (exp_q    << 4); //  9pin 5pin (74LS74)
  unsigned int f = (exp_qn   << 10) | (exp_qn   << 5); //  8pin 6pin (74LS74)
  return apply_test( (~DFF_IO_DIR & (a | b | c | d)), (e | f), DFF_IO_DIR );
}
// JKFF (74LS107)
unsigned int apply_to_4input2( byte test_clk, byte test_clr, byte test_j, byte test_k, byte exp_q, byte exp_qn ) {
  unsigned int a = (test_clk << 14) | (test_clk << 11); // 12pin  9pin (74LS107)
  unsigned int b = (test_clr << 15) | (test_clr << 12); // 13pin 10pin (74LS107)
  unsigned int c = (test_j   << 10) | (test_j   <<  0); //  8pin  1pin (74LS107)
  unsigned int d = (test_k   << 13) | (test_k   <<  3); // 11pin 4pin (74LS107)
  unsigned int e = (exp_q    <<  4) | (exp_q    <<  2); //  5pin 3pin (74LS107)
  unsigned int f = (exp_qn   <<  5) | (exp_qn   <<  1); //  6pin 2pin (74LS107)
  return apply_test( (~JKF_IO_DIR & (a | b | c | d)), (e | f), JKF_IO_DIR );
}
unsigned int apply_test( unsigned test, unsigned e, unsigned io_dir ) {
  write_I2C_port(0x00, byte(io_dir >> 8)); // I/O Direction A 0:output 1:input
  write_I2C_port(0x01, byte(io_dir));      // I/O Direction B 0:output 1:input
  write_I2C_port(GPIOA, test >> 8); // テストパターンが入ったA7-A0をエキスパンダに転送
  write_I2C_port(GPIOB, test >> 0); // テストパターンが入ったB7-B0をエキスパンダに転送
  delay(100);
  unsigned int data = read_I2C_port();
  unsigned int response = data & io_dir & 0b1111110000111111; // 出力応答だけ取り出す
  unsigned int result = response ^ e;        // 期待値とのxorをとって誤ってたら1で残る
  Serial.println("");
  serial_print_bin("Test:", test);
  serial_print_bin("Exp: ", e);
  serial_print_bin("Rspn:", response);

  //  serial_print_bin("IODI:", io_dir);
  //  serial_print_bin("Data:", data);
  //  serial_print_bin("Rslt:", result);

  return result;
}


void serial_print_bin( char* note, unsigned int data) {
  char str[17];
  Serial.print(note);
  for ( int i = 15 ; i >= 0; i-- ) {
    str[i] = (data & 1 << i) ? '1' : '0';
  }
  str[16] = '\0';
  Serial.println(str);
  //  Serial.println(data, BIN);
}


char* int2bin_by_str( char* str, unsigned int data) {
  for ( int i = 15 ; i >= 0; i-- ) {
    str[i] = (data & 1 << i) ? '1' : '0';
  }
  str[16] = '\0';
}

void lcd_print_mode() {
  char str[17];
  lcd.setCursor(0, 0);
  lcd.print(   "Tester for 74xx "); // max 16 character
  lcd.setCursor(0, 1);
  sprintf(str, " -> %s", ic_type[mode]);
  str[16] = '\0';
  //  Serial.println(str);
  lcd.print(str);
  lcd_mode = mode;
}

void lcd_print_result( char* result, unsigned int response ) {
  char str[17];
  lcd.setCursor(0, 0);
  sprintf(str, "Result %c%c%c -> %s", ic_type[mode][0], ic_type[mode][1], ic_type[mode][2], result);
  str[16] = '\0';
  Serial.println(str);
  lcd.print(str);
  lcd.setCursor(0, 1);
  int2bin_by_str(str, response);
  lcd.print(str);
}


char* get_ic_name3( int type ) {
  //  strncpy( str_type, ic_type[type], 3 );
  selected_ic[0] = ic_type[type][0];
  selected_ic[1] = ic_type[type][1];
  selected_ic[2] = ic_type[type][2];
  selected_ic[3] = '\0';
  Serial.println(selected_ic);
  return selected_ic;
}

// I2C I/O expander function
void write_I2C_port(int address, byte value) {
  Wire.beginTransmission(I2C_S_ADDR); // set I2C target
  Wire.write(address);
  Wire.write(value);
  Wire.endTransmission();
}
unsigned int read_I2C_port() {
  byte byte_7_0;
  byte byte_15_8;
  Wire.beginTransmission(I2C_S_ADDR); // set I2C target
  Wire.write(GPIOA);
  Wire.endTransmission();
  Wire.requestFrom(I2C_S_ADDR, 2);
  byte_15_8 = Wire.read();
  byte_7_0  = Wire.read();
  return ( (int)byte_15_8 << 8 | (int)byte_7_0 ); // 読み取った値をくっつけて返す(A7-A0B7-B0)
}

// interupt
void change_mode() {
  ct = millis();
  if ( (ct - pt) > 200 ) {
    mode = (mode + 1) % NO_TYPES;
  }
  pt = ct;
  flag_test = false;
}

void start_test() {
  flag_test = true;
}
