// Built by Flávio Souza.

#include "system.h"
#include "Camera.h"
#include "Connection.h"
#include "Storage.h"

//teste
static const uint8_t DUMMY_JPG[] = {
  0xFF,0xD8,              // SOI
  0xFF,0xE0, 0x00,0x10,   // APP0, length
  'J','F','I','F',0x00,   // "JFIF"
  0x01,0x01,              // version
  0x00,                   // units
  0x00,0x01,0x00,0x01,    // Xdensity, Ydensity
  0x00,0x00,              // Xthumbnail, Ythumbnail
  // padding pra chegar >64 bytes
  0xFF,0xDB,0x00,0x43, 0x00,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  0xFF,0xD9              // EOI
};

void setup()
{
    // Desativa a detecção de brown-out
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);
    
  
    unsigned long start = millis();
    /* Camera espera a deteccao
    do monitor antes de inicializar, esperando 
    ate 5 segundos*/
    while (!Serial && (millis() - start < 5000)) { 
    delay(10);
    Serial.println("XIAO is connected and talking!");
    }

    Connection.setup();

    Connection.sendImageBuffer(321.0, DUMMY_JPG, sizeof(DUMMY_JPG)); //teste

    // Configura resolução do ADC e pinos
    analogReadResolution(9); // 2^9 = 512 bytes
    setupPinout();    

    // Configura deep sleep para despertar a cada sleep_period minutos
    esp_sleep_enable_timer_wakeup(SLEEP_PERIOD);
    delay(5000);
    Serial.println("\nSetup ESP32 to sleep for every " + String(sleep_period) + " minutes");
    Serial.printf("PSRAM: %dMB\n", esp_spiram_get_size() / 1048576);

    state = moisture_read;

    Connection.setup();
    Storage.setup();  
}

void loop()
{
    handleStates();
}
