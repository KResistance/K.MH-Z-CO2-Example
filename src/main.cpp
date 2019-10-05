// based on https://mysku.ru/blog/aliexpress/59397.html
#include<Arduino.h>
#include <SoftwareSerial.h>
#define pwmPin D2
SoftwareSerial swSerial(D5, D6); // RX, TX
// SoftwareSerial swSerial; // for new ESS version
byte writeCommandConst[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

byte calcCRC(byte data[])
{
	byte i;
	byte crc = 0;
	for (i = 1; i < 8; i++)
	{
		crc += data[i];
	}
	crc = 255 - crc;
	crc++;

	return crc;
}

// not working now
// struct CommandResult
// { 
//     bool isCRCCorrect; 
//     byte computedCRC;
//     byte returnedCRC;
//     byte response[9];
// };

// not working now
// CommandResult* writeCommand(byte command[], byte validResponce){
//   CommandResult * result = new CommandResult();
//   result->isCRCCorrect = false;
//   command[8] = calcCRC(command);
//   swSerial.write(result->response, 9);
//   swSerial.flush();
//   swSerial.readBytes(result->response, 9);
//   result->returnedCRC = result->response[8];
//   result->computedCRC = calcCRC(result->response);
//   if (result->response[0] == 0xFF && result->response[1] == validResponce && result->returnedCRC == result->computedCRC)
//     result->isCRCCorrect = true;
//   return result;
// }
// void turnOffSelfCalibration(){
//   CommandResult * result = writeCommand(writeCommandConst, 0x79);
//   if (result->isCRCCorrect) 
//     Serial.println("Self calibration was turned off!");
//   else 
//     Serial.println("Range CRC error: " + String(result->computedCRC) + " / "+ String(result->returnedCRC));
// }

void turnOffSelfCalibration(){
  byte command[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  command[8] = calcCRC(command);
  unsigned char response[9]; 
  swSerial.write(command,9);
  swSerial.flush();
  swSerial.readBytes(response, 9);
  byte responseCrc = calcCRC(response);
  if ( !(response[0] == 0xFF && response[1] == 0x79 && response[8] == responseCrc) ) {
    Serial.println("Range CRC error: " + String(responseCrc) + " / "+ String(response[8]));
  } else {
    Serial.println("Self calibration was turned off!");
  }
}

void setup() {
  Serial.begin(115200); 
  swSerial.begin(9600); 
  // swSerial.begin(9600, D5, D6, SoftwareSerialConfig::SWSERIAL_8N1, false, 256); // for new ESS version: rate, RX, TX
  pinMode(pwmPin, INPUT);

  /*
  Источник - https://revspace.nl/MHZ19 и https://github.com/strange-v/MHZ19/blob/ba883d166eaba26b0d6f68c1cd0e664cd405a1ad/MHZ19.cpp#L58
   1000 ppm range: 0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x03, 0xE8, 
   2000 ppm range: 0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x07, 0xD0, 0x8F
   3000 ppm range: 0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x0B, 0xB8, 
   5000 ppm range: 0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0xCB
   10000 ppm range: 0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x27, 0x10, 
  */

  // Этот вариант ("A") с записью команды в 6й и 7й байт - работает
  //           bytes:                         3     4           6     7
  byte setrangeA_cmd[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0xCB}; // задаёт диапазон 0 - 5000ppm
  setrangeA_cmd[8] = calcCRC(setrangeA_cmd);
  unsigned char setrangeA_response[9]; 
  swSerial.write(setrangeA_cmd,9);
  swSerial.flush();
  swSerial.readBytes(setrangeA_response, 9);
  byte setrangeA_crc = calcCRC(setrangeA_response);
  if ( !(setrangeA_response[0] == 0xFF && setrangeA_response[1] == 0x99 && setrangeA_response[8] == setrangeA_crc) ) {
    Serial.println("Range CRC error: " + String(setrangeA_crc) + " / "+ String(setrangeA_response[8]) + " (bytes 6 and 7)");
  } else {
    Serial.println("Range was set! (bytes 6 and 7)");
  }
  delay(1000);
  turnOffSelfCalibration();
  delay(1000);
}

void loop() {
  byte measure_cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
  unsigned char measure_response[9]; 
  unsigned long th, tl, ppm = 0, ppm2 = 0;

  // ***** узнаём концентрацию CO2 через UART: ***** 
  while (swSerial.available() > 0) {swSerial.read(); }
  swSerial.write(measure_cmd,9);
  swSerial.flush();
  swSerial.readBytes(measure_response, 9);
  byte crc = calcCRC(measure_response);
  if ( !(measure_response[0] == 0xFF && measure_response[1] == 0x86 && measure_response[8] == crc) ) {
    Serial.println("CRC error: " + String(crc) + " / "+ String(measure_response[8]));
  } 
  unsigned int responseHigh = (unsigned int) measure_response[2];
  unsigned int responseLow = (unsigned int) measure_response[3];
  ppm = (256*responseHigh) + responseLow;

  // *****  узнаём концентрацию CO2 через PWM: ***** 
  do {
    th = pulseIn(pwmPin, HIGH, 1004000) / 1000;
    tl = 1004 - th;
    ppm2 =  2000 * (th-2)/(th+tl-4); // расчёт для диапазона от 0 до 2000ppm 
  } while (th == 0);

  Serial.print(ppm);
  Serial.print(" <- ppm (UART) ");
  Serial.print((ppm/5)*2);
  Serial.println(" <- two fifths of it (real value)"); // Потом пришло озарение
  Serial.print(th);
  Serial.println(" <- Milliseconds PWM is HIGH");
  Serial.print(ppm2);
  Serial.println(" <- ppm2 (PWM) with valid range");

  Serial.println("-----------");
  delay(5000);
}