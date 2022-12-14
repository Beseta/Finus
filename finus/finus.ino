#include <LiquidCrystal.h>
#include <string.h>
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

#define LENGTH 512
float noteFreqs[6] = {77.78, 103.83, 138.59, 185.00, 233.08, 311.13 };
char strings[6] = {'E', 'A', 'D', 'G', 'B', 'H'};
byte rawData[LENGTH];
int count;

// Sample Frequency in kHz
const float sample_freq = 8919;
// const float sample_freq = 22050;

int len = sizeof(rawData);
int i, k;
long sum, sum_old;
int thresh = 0;
float freq_per = 0;
byte pd_state = 0;

float getCents(float freq, float targetFreq) {
  return 1200 * (log(freq / targetFreq) / log(2));
}

void getNote(float freq) {
  int left = 0, right = 5;

  while (left <= right) {
    int mid = (left + right) / 2;
    if (noteFreqs[mid] == freq) {
      Serial.println(strings[mid]);
    } else if (noteFreqs[mid] < freq) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  if (abs(noteFreqs[left] - freq) > abs(noteFreqs[right] - freq)) {
    float targetFreq = noteFreqs[right];
    Serial.print(strings[right]);
    targetFreq - freq < 0 ? Serial.println(", +" + String(getCents(freq, targetFreq)) + " cents") : Serial.println(", " + String(getCents(freq, targetFreq)) + " cents");

    // PRINT TO LCD
    lcd.setCursor(7, 0);
    lcd.print(strings[right]);
    lcd.setCursor(0, 1);
    targetFreq - freq < 0 ? lcd.print("+" + String(getCents(freq, targetFreq)) + " cents") : lcd.print(String(getCents(freq, targetFreq)) + " cents");
  } else {
    float targetFreq = noteFreqs[left];
    Serial.print(strings[left]);
    targetFreq - freq < 0 ? Serial.println(", +" + String(getCents(freq, targetFreq)) + " cents") : Serial.println(", " + String(getCents(freq, targetFreq)) + " cents");

    // PRINT TO LCD
    lcd.setCursor(7, 0);
    lcd.print(strings[left]);
    lcd.setCursor(0, 1);
    targetFreq - freq < 0 ? lcd.print("+" + String(getCents(freq, targetFreq)) + " cents") : lcd.print(String(getCents(freq, targetFreq)) + " cents");
  }
}

void setup() {
  lcd.begin(16, 2);
  analogReference(EXTERNAL);  // Connect to 3.3V
  analogRead(A0);
  Serial.begin(115200);
  count = 0;
}


void loop() {
  if (count < LENGTH) {
    count++;
    rawData[count] = analogRead(A0) >> 2;
  } else {
    sum = 0;
    pd_state = 0;
    int period = 0;
    for (i = 0; i < len; i++) {
      // Autocorrelation
      sum_old = sum;
      sum = 0;
      for (k = 0; k < len - i; k++) sum += (rawData[k] - 128) * (rawData[k + i] - 128) / 256;

      // Peak Detect State Machine
      if (pd_state == 2 && (sum - sum_old) <= 0) {
        period = i;
        pd_state = 3;
      }
      if (pd_state == 1 && (sum > thresh) && (sum - sum_old) > 0) pd_state = 2;
      if (!i) {
        thresh = sum * 0.5;
        pd_state = 1;
      }
    }
    // Frequency identified in Hz
    if (thresh > 100) {
      freq_per = sample_freq / period;
      lcd.clear();
      getNote(freq_per);
    }
    count = 0;
  }
}