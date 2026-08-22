// Test toolchain STM32 Nucleo F401RE — clignote LD2 (PA5)
void setup() { pinMode(LED_BUILTIN, OUTPUT); }
void loop()  { digitalWrite(LED_BUILTIN, HIGH); delay(300);
               digitalWrite(LED_BUILTIN, LOW);  delay(300); }
