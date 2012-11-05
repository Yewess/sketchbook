
/* I/O constants */
const int currentSensePin = A0;
const int sampleDelay = 1000; // uS
const int sampleCount = 10;

void setup(void) {
    Serial.begin(9600);
    Serial.println("setup()");

    pinMode(currentSensePin, INPUT);
    Serial.println("loop()");
}

void loop(void) {
    int samples[sampleCount] = {0};
    int sampleIndex = 0;
    int dotIndex = 0;

    for (sampleIndex=0; sampleIndex < sampleCount; sampleIndex++) {
        samples[sampleIndex] = map( analogRead(currentSensePin), 0, 1023, -19, 19);
        delayMicroseconds(sampleDelay);
    }

    for (sampleIndex=0; sampleIndex < sampleCount; sampleIndex++) {
        Serial.print(samples[sampleIndex]); Serial.print("\t");
        if (samples[sampleIndex] > 0) {
            Serial.print("                   "); // Space for -
        }
        for (dotIndex = abs(samples[sampleIndex]); dotIndex > 0; dotIndex--) {
            if (samples[sampleIndex] < 0) {
                Serial.print("-");
            } else {
                Serial.print("+");
            }
        }
        Serial.println();  
    }
    delay(1000);    
}
