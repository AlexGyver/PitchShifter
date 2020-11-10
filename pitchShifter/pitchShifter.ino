#define THRESHOLD 60  // порог отключения звука
// вход звука на A0
// потенциометр питча на А1
// выход звука с D9

// ручной уровень искажения. 0 - управление внешней крутилкой
// ненулевое: значение 2-17, где 2 - макс. искажение, 17 - искажение отключено
#define MANUAL_PITCH 3

// =====================================================
void setup() {
  //Serial.begin(115200);
  // таймер 1 режим 7
  TCCR1A = bit(COM1A1) | bit(WGM10) | bit(WGM11); // шим 10 бит канал А
  TCCR1B = bit(WGM12) | bit(WGM10); // частота 31 кгц fast pwm
  TIMSK1 = bit(TOIE1);              // прерывания по ovf (bottom)

  ADMUX = (EXTERNAL << 6) | 0;      // внешний реф, ацп 0
  ADCSRA = bit(ADEN) | bit(ADPS2);  // x16
  ADCSRB = 0;

  pinMode(9, OUTPUT);
}

// =====================================================
#define BUF_SIZE 512
#define OVERLAP 16
bool flag;
volatile byte shift = 5;
volatile uint16_t counter = 0;
volatile int input = 0, output = 0;
volatile int writeAddr = 0, readAddr = 0;
volatile int pitchBuf[BUF_SIZE];
volatile int inputBuf[256];
volatile byte inputCounter = 0;
volatile int maxSound, newMaxSound;

// 240 гц??

ISR(TIMER1_OVF_vect) {
#if (MANUAL_PITCH > 0)
  shift = MANUAL_PITCH;
  ADMUX = (EXTERNAL << 6) | 1;            // внешний реф, ацп А1 (для крутилки)
  inputBuf[inputCounter++] = ADC;         // пишем в буфер звука
#else
  if ((counter & 0x7FF) == 0x7FF - 1) {     // каждый 2047 тик
    ADMUX = (EXTERNAL << 6) | 1;            // внешний реф, ацп А1 (для крутилки)
    //input = ADC;                          // читаем прошлый АЦП как input
    inputBuf[inputCounter++] = ADC;         // пишем в буфер звука
  } else if ((counter & 0x7FF) == 0x7FF) {  // каждый 2048 тик
    ADMUX = (EXTERNAL << 6) | 0;            // внешний реф, ацп А0 (для звука)
    shift = (ADC >> 6) + 2;                 // читаем прошлый АЦП как настройку и делим на 64 (2-17)
  } else {
    //input = ADC;                          // читаем АЦП как input
    inputBuf[inputCounter++] = ADC;         // пишем в буфер звука
  }
#endif

  bitSet(ADCSRA, ADSC);                         // начинаем новое чтение с ацп
  input = inputBuf[byte(inputCounter - 255)];   // запаздываем на 255 сэмплов

  if (input > newMaxSound) newMaxSound = input; // поиск нового максимума
  pitchBuf[writeAddr] = input;                  // пишем в буфер питча

  if (flag) OCR1A = output; // заполнение ШИМ (0-1023), если громкость выше порога

  // сам алгоритм питч шифта основан на
  // https://github.com/nootropicdesign/audio-hacker/
  counter++;
  if (counter % shift != 0 || shift == 17) {    // пропуск сэмплов
    output = pitchBuf[readAddr];
    readAddr++;
    int distance = BUF_SIZE - writeAddr;
    if (distance <= OVERLAP) {
      int aver = (output * distance) + (input * (OVERLAP - distance));
      output = aver >> 4;
    }
  }

  writeAddr++;
  if (writeAddr >= BUF_SIZE) {
    writeAddr = 0;
    readAddr = 0;
    maxSound = newMaxSound;
    newMaxSound = 0;
  }
}

void loop() {
  static int filtSound, filtMax;
  filtMax = (13 * filtMax + 3 * maxSound) >> 4;   // фильтруем пики
  filtSound = (15 * filtSound + 1 * input) >> 4;  // фильтруем сам сигнал
  flag = (filtMax - filtSound) > THRESHOLD;       // порог включения генерации

  /*Serial.print(filtSound);
    Serial.print(',');
    Serial.print(filtMax);
    Serial.print(',');*/
  /*Serial.print(filtMax - filtSound);
    Serial.print(',');
    Serial.println(THRESHOLD);*/

  delay(3);
}
