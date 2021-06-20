// Модулятор голоса на Arduino
// AlexGyver, 2020 https://alexgyver.ru/
// Спасибо Nich1con за настройку таймера и АЦП

#define THRESHOLD 70  // порог отключения звука
// вход звука на A0
// потенциометр питча на А1
// выход звука с D9
// Aref подключен к 3V3, АЦП EXTERNAL!

// ручной уровень искажения. 0 - управление внешней крутилкой
// ненулевое: значение 2-17, где 2 - макс. искажение, 17 - искажение отключено
#define MANUAL_SKIP 0

// раскомментируй для вывода графиков в порт
//#define DEBUG_SERIAL

// =====================================================
#define BUF_SIZE 300
#define OVERLAP 16
bool soundFlag = 0;
volatile byte skip = 0;
volatile uint16_t counter = 0;
volatile uint16_t input = 0, output = 0;
volatile uint16_t writeAddr = 0, readAddr = 0;
volatile uint16_t inputBuf[BUF_SIZE];
volatile uint16_t maxSound, newMaxSound;

void setup() {
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
#endif
  // таймер 1 режим 7
  TCCR1A = bit(COM1A1) | bit(WGM10) | bit(WGM11); // шим 10 бит канал А
  TCCR1B = bit(WGM12) | bit(WGM10); // частота 31 кгц fast pwm
  TIMSK1 = bit(TOIE1);              // прерывания по ovf (bottom)

  ADMUX = (EXTERNAL << 6) | 0;      // внешний реф, ацп A0
  ADCSRA = bit(ADEN) | bit(ADPS2);  // x16
  ADCSRB = 0;

  pinMode(9, OUTPUT);
  skip = MANUAL_SKIP;
}

// =====================================================

ISR(TIMER1_OVF_vect) {

#if (MANUAL_SKIP > 0)
  input = ADC;
#else
  if ((counter & 0x7FF) == 0x7FF - 1) {     // каждый 2047 тик
    bitWrite(ADMUX, 0, 1);
    input = ADC;
  } else if ((counter & 0x7FF) == 0x7FF) {  // каждый 2048 тик
    bitWrite(ADMUX, 0, 0);
    skip = (ADC >> 6) + 2;   // читаем прошлый АЦП как настройку и делим на 64 (2-17)
  } else {
    input = ADC;
  }
#endif

  bitSet(ADCSRA, ADSC);                         // начинаем новое чтение с ацп
  newMaxSound = max(newMaxSound, input);        // ищем максимум за выборку
  inputBuf[writeAddr] = input;                  // пишем в буфер

  if (soundFlag) OCR1A = (skip == 17) ? input : output;  // заполнение ШИМ (0-1023), если громкость выше порога

  // сам алгоритм питч шифта основан на
  // https://github.com/nootropicdesign/audio-hacker/
  if (++counter % skip) {    // пропуск сэмплов
    output = inputBuf[readAddr++];
    int distance = BUF_SIZE - writeAddr;
    if (distance <= OVERLAP) {
      int aver = (output * distance) + (OVERLAP - distance) * input;
      output = aver >> 4;
    }
  }

  if (++writeAddr >= BUF_SIZE) {
    maxSound = newMaxSound;
    newMaxSound = writeAddr = readAddr = 0;
  }
}

void loop() {
  static int filtSound, filtMax;
  filtMax = (13 * filtMax + 3 * maxSound) >> 4;   // фильтруем пики
  filtSound = (15 * filtSound + 1 * input) >> 4;  // фильтруем сам сигнал
  soundFlag = (filtMax - filtSound) > THRESHOLD;  // порог включения генерации
  delay(3);
  
#ifdef DEBUG_SERIAL
  Serial.print(filtSound);
  Serial.print(',');
  Serial.print(filtMax);
  Serial.print(',');
  Serial.print(filtMax - filtSound);
  Serial.print(',');
  Serial.println(THRESHOLD);
#endif
}
