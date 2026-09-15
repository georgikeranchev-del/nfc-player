#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include <Adafruit_PN532.h>
#include <FastLED.h>

#include "AudioFileSourceSD.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"


// ============================================================
// PINOUT
// ============================================================

// SD card - HSPI
#define SD_CS       13
#define SD_SCK      14
#define SD_MISO     16
#define SD_MOSI     15

// PN532 - Software SPI
#define PN532_SS    5
#define PN532_SCK   18
#define PN532_MISO  19
#define PN532_MOSI  23

// DC Motor
#define MOTOR_PIN   27

// MAX98357A
#define I2S_LRC     25
#define I2S_BCLK    26
#define I2S_DOUT    22

// Rotary Encoder
#define ENCODER_CLK 32
#define ENCODER_DT  33
#define ENCODER_SW  4

// WS2812B LED ring
#define LED_PIN     21
#define NUM_LEDS    24


// ============================================================
// SETTINGS
// ============================================================

const uint8_t MAX_LOOPS = 3;

// SD speed
const uint32_t SD_SPEED = 10000000;

// AudioFileSourceBuffer size
const size_t AUDIO_BUFFER_SIZE = 8192;

// NFC scan intervals
const uint32_t NFC_SCAN_IDLE    = 150;
const uint32_t NFC_SCAN_PLAYING = 200;

// Number of consecutive failed NFC reads
// before we consider the tag removed.
const uint8_t NFC_MISSING_LIMIT = 2;

// Initial volume
const uint8_t INITIAL_VOLUME = 12;

// Volume limits
const uint8_t MIN_VOLUME = 0;
const uint8_t MAX_VOLUME = 21;

// LED brightness
const uint8_t LED_BRIGHTNESS = 120;


// ============================================================
// OBJECTS
// ============================================================

// Separate SPI bus for SD
SPIClass sdSPI(HSPI);

// PN532 software SPI
Adafruit_PN532 nfc(
  PN532_SCK,
  PN532_MISO,
  PN532_MOSI,
  PN532_SS
);

// Audio objects
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *file = nullptr;
AudioFileSourceBuffer *audioBuffer = nullptr;
AudioOutputI2S *out = nullptr;

// WS2812B
CRGB leds[NUM_LEDS];


// ============================================================
// GLOBAL VARIABLES
// ============================================================

char currentUID[15] = "";
char playingFile[40] = "";

volatile bool isPlaying = false;

uint8_t loopCount = 0;
uint8_t volume = INITIAL_VOLUME;

int lastClkState = HIGH;


// ============================================================
// NFC TASK VARIABLES
// ============================================================

volatile bool nfcTaskRunning = true;

volatile bool nfcTagDetected = false;
volatile bool nfcTagRemoved = false;

char nfcUID[15] = "";

uint8_t missingReads = 0;

TaskHandle_t nfcTaskHandle = nullptr;

SemaphoreHandle_t nfcMutex = nullptr;


// ============================================================
// LED VARIABLES
// ============================================================

enum LEDMode
{
  LED_STANDBY,
  LED_STARTING,
  LED_PLAYING,
  LED_STOPPING,
  LED_LOOP
};

LEDMode ledMode = LED_STANDBY;

uint32_t ledModeStart = 0;
uint32_t lastLEDUpdate = 0;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void stopPlayback();

bool readNFCUID(char *uid, size_t uidSize);

bool playTrack(const char *filename);

void nfcTask(void *parameter);

void enterDeepSleep();

void handleEncoder();

void setLEDMode(LEDMode mode);

void updateLEDs();

void clearLEDs();


// ============================================================
// LED MODE
// ============================================================

void setLEDMode(LEDMode mode)
{
  ledMode = mode;
  ledModeStart = millis();
}


// ============================================================
// CLEAR LEDS
// ============================================================

void clearLEDs()
{
  fill_solid(
    leds,
    NUM_LEDS,
    CRGB::Black
  );

  FastLED.show();
}


// ============================================================
// LED - STANDBY
//
// Slow breathing effect.
// ============================================================

void updateLEDStandby()
{
  uint8_t brightness = beatsin8(
    10,
    5,
    35
  );

  fill_solid(
    leds,
    NUM_LEDS,
    CRGB(
      brightness,
      brightness,
      brightness
    )
  );
}


// ============================================================
// LED - STARTING
//
// One light travels around the ring.
// ============================================================

void updateLEDStarting()
{
  uint32_t elapsed =
    millis() - ledModeStart;

  uint8_t position =
    (elapsed / 35) % NUM_LEDS;

  fadeToBlackBy(
    leds,
    NUM_LEDS,
    45
  );

  leds[position] =
    CRGB::White;

  leds[
    (position + NUM_LEDS - 1) % NUM_LEDS
  ] = CRGB(80, 80, 80);

  leds[
    (position + NUM_LEDS - 2) % NUM_LEDS
  ] = CRGB(30, 30, 30);
}


// ============================================================
// LED - PLAYING
//
// Rotating pulse around the ring.
// ============================================================

void updateLEDPlaying()
{
  fadeToBlackBy(
    leds,
    NUM_LEDS,
    35
  );

  uint8_t position =
    (millis() / 55) % NUM_LEDS;

  leds[position] =
    CRGB::White;

  leds[
    (position + NUM_LEDS - 1) % NUM_LEDS
  ] = CRGB(90, 90, 90);

  leds[
    (position + NUM_LEDS - 2) % NUM_LEDS
  ] = CRGB(35, 35, 35);

  // Very subtle overall pulse
  uint8_t pulse =
    beatsin8(
      12,
      0,
      18
    );

  if (pulse > 0)
  {
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
      leds[i].nscale8(
        255 - pulse
      );
    }
  }
}


// ============================================================
// LED - LOOP
//
// Short flash when a track starts another loop.
// ============================================================

void updateLEDLoop()
{
  uint32_t elapsed =
    millis() - ledModeStart;

  uint8_t brightness = 0;

  if (elapsed < 250)
  {
    brightness =
      map(
        elapsed,
        0,
        250,
        0,
        255
      );
  }
  else if (elapsed < 500)
  {
    brightness =
      map(
        elapsed,
        250,
        500,
        255,
        0
      );
  }
  else
  {
    setLEDMode(LED_PLAYING);
    return;
  }

  fill_solid(
    leds,
    NUM_LEDS,
    CRGB(
      brightness,
      brightness,
      brightness
    )
  );
}


// ============================================================
// LED - STOPPING
//
// Quick fade out.
// ============================================================

void updateLEDStopping()
{
  uint32_t elapsed =
    millis() - ledModeStart;

  if (elapsed < 350)
  {
    uint8_t brightness =
      map(
        elapsed,
        0,
        350,
        255,
        0
      );

    fill_solid(
      leds,
      NUM_LEDS,
      CRGB(
        brightness,
        brightness,
        brightness
      )
    );
  }
  else
  {
    setLEDMode(LED_STANDBY);
  }
}


// ============================================================
// UPDATE LEDS
//
// Non-blocking.
// Called from main loop.
// ============================================================

void updateLEDs()
{
  uint32_t now = millis();

  // Update approximately every 20 ms
  if (
    now - lastLEDUpdate < 20
  )
  {
    return;
  }

  lastLEDUpdate = now;


  switch (ledMode)
  {
    case LED_STANDBY:
      updateLEDStandby();
      break;

    case LED_STARTING:
      updateLEDStarting();
      break;

    case LED_PLAYING:
      updateLEDPlaying();
      break;

    case LED_LOOP:
      updateLEDLoop();
      break;

    case LED_STOPPING:
      updateLEDStopping();
      break;
  }


  FastLED.show();
}


// ============================================================
// STOP PLAYBACK
// ============================================================

void stopPlayback()
{
  Serial.println();
  Serial.println("Stopping playback...");

  // Stop MP3 decoder
  if (mp3 != nullptr)
  {
    if (mp3->isRunning())
    {
      mp3->stop();
    }

    delete mp3;
    mp3 = nullptr;
  }

  // Delete audio buffer
  if (audioBuffer != nullptr)
  {
    delete audioBuffer;
    audioBuffer = nullptr;
  }

  // Close/delete file
  if (file != nullptr)
  {
    file->close();

    delete file;
    file = nullptr;
  }

  // Stop motor
  digitalWrite(
    MOTOR_PIN,
    LOW
  );

  isPlaying = false;

  Serial.println("Playback stopped.");
}


// ============================================================
// DEEP SLEEP
// ============================================================

void enterDeepSleep()
{
  Serial.println();
  Serial.println("================================");
  Serial.println("Playback finished.");
  Serial.println("Entering deep sleep...");
  Serial.println("Press encoder button to wake.");
  Serial.println("================================");

  clearLEDs();

  delay(200);

  // Wake when encoder switch is pressed
  esp_sleep_enable_ext0_wakeup(
    (gpio_num_t)ENCODER_SW,
    0
  );

  esp_deep_sleep_start();
}


// ============================================================
// READ NFC UID
// ============================================================

bool readNFCUID(
  char *uid,
  size_t uidSize
)
{
  uint8_t uidBuffer[10];
  uint8_t uidLength = 0;

  bool success =
    nfc.readPassiveTargetID(
      PN532_MIFARE_ISO14443A,
      uidBuffer,
      &uidLength,
      50
    );

  if (!success)
  {
    return false;
  }

  if (
    uidLength == 0 ||
    uidLength > 7
  )
  {
    return false;
  }

  // Convert UID to uppercase HEX string
  size_t pos = 0;

  for (
    uint8_t i = 0;
    i < uidLength;
    i++
  )
  {
    if (
      pos + 2 >= uidSize
    )
    {
      return false;
    }

    sprintf(
      &uid[pos],
      "%02X",
      uidBuffer[i]
    );

    pos += 2;
  }

  uid[pos] = '\0';

  return true;
}


// ============================================================
// PLAY TRACK
// ============================================================

bool playTrack(
  const char *filename
)
{
  Serial.println();
  Serial.println("================================");
  Serial.print("Starting track: ");
  Serial.println(filename);
  Serial.println("================================");

  // Stop anything currently playing
  stopPlayback();

  strncpy(
    playingFile,
    filename,
    sizeof(playingFile) - 1
  );

  playingFile[
    sizeof(playingFile) - 1
  ] = '\0';


  // Check file exists
  if (!SD.exists(playingFile))
  {
    Serial.print(
      "ERROR: File not found: "
    );

    Serial.println(
      playingFile
    );

    return false;
  }


  // ----------------------------------------------------------
  // Start motor
  // ----------------------------------------------------------

  digitalWrite(
    MOTOR_PIN,
    HIGH
  );

  // Give platter a little time to start rotating
  delay(300);


  // ----------------------------------------------------------
  // LED starting animation
  // ----------------------------------------------------------

  setLEDMode(
    LED_STARTING
  );


  // ----------------------------------------------------------
  // Open MP3 file
  // ----------------------------------------------------------

  file =
    new AudioFileSourceSD(
      playingFile
    );

  if (file == nullptr)
  {
    Serial.println(
      "ERROR: Cannot allocate AudioFileSourceSD."
    );

    digitalWrite(
      MOTOR_PIN,
      LOW
    );

    return false;
  }


  // ----------------------------------------------------------
  // Create RAM audio buffer
  // ----------------------------------------------------------

  audioBuffer =
    new AudioFileSourceBuffer(
      file,
      AUDIO_BUFFER_SIZE
    );

  if (audioBuffer == nullptr)
  {
    Serial.println(
      "ERROR: Cannot allocate AudioFileSourceBuffer."
    );

    delete file;
    file = nullptr;

    digitalWrite(
      MOTOR_PIN,
      LOW
    );

    return false;
  }


  // ----------------------------------------------------------
  // Create MP3 decoder
  // ----------------------------------------------------------

  mp3 =
    new AudioGeneratorMP3();

  if (mp3 == nullptr)
  {
    Serial.println(
      "ERROR: Cannot allocate AudioGeneratorMP3."
    );

    delete audioBuffer;
    audioBuffer = nullptr;

    delete file;
    file = nullptr;

    digitalWrite(
      MOTOR_PIN,
      LOW
    );

    return false;
  }


  // ----------------------------------------------------------
  // Start MP3 decoder
  // ----------------------------------------------------------

  if (
    !mp3->begin(
      audioBuffer,
      out
    )
  )
  {
    Serial.println(
      "ERROR: MP3 begin() failed."
    );

    delete mp3;
    mp3 = nullptr;

    delete audioBuffer;
    audioBuffer = nullptr;

    delete file;
    file = nullptr;

    digitalWrite(
      MOTOR_PIN,
      LOW
    );

    return false;
  }


  isPlaying = true;

  // Starting animation will naturally
  // transition into playing animation.
  setLEDMode(
    LED_STARTING
  );

  Serial.println(
    "MP3 playback started."
  );

  return true;
}


// ============================================================
// NFC TASK
//
// Runs on CORE 0
// ============================================================

void nfcTask(
  void *parameter
)
{
  Serial.println(
    "NFC task started on Core 0."
  );

  while (nfcTaskRunning)
  {
    char detectedUID[15] = "";

    bool detected =
      readNFCUID(
        detectedUID,
        sizeof(detectedUID)
      );


    // ========================================================
    // TAG FOUND
    // ========================================================

    if (detected)
    {
      missingReads = 0;


      // If this is a NEW tag
      if (
        strcmp(
          detectedUID,
          currentUID
        ) != 0
      )
      {
        Serial.print(
          "NFC tag detected: "
        );

        Serial.println(
          detectedUID
        );

        if (nfcMutex != nullptr)
        {
          xSemaphoreTake(
            nfcMutex,
            portMAX_DELAY
          );
        }

        strncpy(
          nfcUID,
          detectedUID,
          sizeof(nfcUID) - 1
        );

        nfcUID[
          sizeof(nfcUID) - 1
        ] = '\0';

        nfcTagDetected = true;

        if (nfcMutex != nullptr)
        {
          xSemaphoreGive(
            nfcMutex
          );
        }
      }
    }


    // ========================================================
    // TAG NOT FOUND
    // ========================================================

    else
    {
      if (
        currentUID[0] != '\0'
      )
      {
        missingReads++;

        if (
          missingReads >=
          NFC_MISSING_LIMIT
        )
        {
          Serial.println(
            "NFC tag removed."
          );

          if (nfcMutex != nullptr)
          {
            xSemaphoreTake(
              nfcMutex,
              portMAX_DELAY
            );
          }

          nfcTagRemoved = true;

          if (nfcMutex != nullptr)
          {
            xSemaphoreGive(
              nfcMutex
            );
          }

          missingReads = 0;
        }
      }
    }


    // ========================================================
    // ADAPTIVE NFC INTERVAL
    // ========================================================

    if (isPlaying)
    {
      vTaskDelay(
        pdMS_TO_TICKS(
          NFC_SCAN_PLAYING
        )
      );
    }
    else
    {
      vTaskDelay(
        pdMS_TO_TICKS(
          NFC_SCAN_IDLE
        )
      );
    }
  }


  Serial.println(
    "NFC task stopped."
  );

  vTaskDelete(
    nullptr
  );
}


// ============================================================
// ENCODER
// ============================================================

void handleEncoder()
{
  int clkState =
    digitalRead(
      ENCODER_CLK
    );

  // Detect CLK transition
  if (
    clkState != lastClkState
  )
  {
    // Only react on falling edge
    if (clkState == LOW)
    {
      int dtState =
        digitalRead(
          ENCODER_DT
        );

      if (
        dtState != clkState
      )
      {
        // Clockwise
        if (
          volume < MAX_VOLUME
        )
        {
          volume++;
        }
      }
      else
      {
        // Counter-clockwise
        if (
          volume > MIN_VOLUME
        )
        {
          volume--;
        }
      }

      float gain =
        (float)volume /
        (float)MAX_VOLUME;

      if (out != nullptr)
      {
        out->SetGain(
          gain
        );
      }

      Serial.print(
        "Volume: "
      );

      Serial.print(
        volume
      );

      Serial.print(
        " / "
      );

      Serial.println(
        MAX_VOLUME
      );
    }

    lastClkState =
      clkState;
  }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );

  delay(1000);

  Serial.println();
  Serial.println();
  Serial.println(
    "======================================"
  );

  Serial.println(
    " NFC VINYL PLAYER v2.1"
  );

  Serial.println(
    " ESP8266Audio + ESP32-WROOM-32"
  );

  Serial.println(
    " WS2812B LED RING"
  );

  Serial.println(
    "======================================"
  );


  // ==========================================================
  // MOTOR
  // ==========================================================

  pinMode(
    MOTOR_PIN,
    OUTPUT
  );

  digitalWrite(
    MOTOR_PIN,
    LOW
  );


  // ==========================================================
  // ENCODER
  // ==========================================================

  pinMode(
    ENCODER_CLK,
    INPUT_PULLUP
  );

  pinMode(
    ENCODER_DT,
    INPUT_PULLUP
  );

  pinMode(
    ENCODER_SW,
    INPUT_PULLUP
  );

  lastClkState =
    digitalRead(
      ENCODER_CLK
    );


  // ==========================================================
  // LED RING
  // ==========================================================

  FastLED.addLeds<
    WS2812B,
    LED_PIN,
    GRB
  >(
    leds,
    NUM_LEDS
  );

  FastLED.setBrightness(
    LED_BRIGHTNESS
  );

  clearLEDs();

  setLEDMode(
    LED_STANDBY
  );


  // ==========================================================
  // SD CARD
  // ==========================================================

  Serial.println();
  Serial.println(
    "Initializing SD card..."
  );

  sdSPI.begin(
    SD_SCK,
    SD_MISO,
    SD_MOSI,
    SD_CS
  );

  if (
    !SD.begin(
      SD_CS,
      sdSPI,
      SD_SPEED
    )
  )
  {
    Serial.println(
      "ERROR: SD initialization failed!"
    );

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "SD initialized successfully."
  );

  Serial.print(
    "SD speed: "
  );

  Serial.print(
    SD_SPEED / 1000000
  );

  Serial.println(
    " MHz"
  );


  // ==========================================================
  // CHECK PLAYER DIRECTORY
  // ==========================================================

  if (
    !SD.exists(
      "/player"
    )
  )
  {
    Serial.println(
      "Creating /player directory..."
    );

    if (
      !SD.mkdir(
        "/player"
      )
    )
    {
      Serial.println(
        "WARNING: Could not create /player."
      );
    }
  }

  Serial.println(
    "Player directory ready."
  );


  // ==========================================================
  // PN532
  // ==========================================================

  Serial.println();
  Serial.println(
    "Initializing PN532..."
  );

  nfc.begin();

  uint32_t versiondata =
    nfc.getFirmwareVersion();

  if (!versiondata)
  {
    Serial.println(
      "ERROR: PN532 not found!"
    );

    while (true)
    {
      delay(1000);
    }
  }

  Serial.print(
    "PN532 firmware: "
  );

  Serial.print(
    (versiondata >> 24) & 0xFF,
    HEX
  );

  Serial.print(
    "."
  );

  Serial.println(
    (versiondata >> 16) & 0xFF,
    HEX
  );


  nfc.SAMConfig();

  Serial.println(
    "PN532 ready."
  );


  // ==========================================================
  // I2S OUTPUT
  // ==========================================================

  Serial.println();
  Serial.println(
    "Initializing I2S..."
  );

  out =
    new AudioOutputI2S();

  if (out == nullptr)
  {
    Serial.println(
      "ERROR: Cannot allocate AudioOutputI2S!"
    );

    while (true)
    {
      delay(1000);
    }
  }

  out->SetPinout(
    I2S_BCLK,
    I2S_LRC,
    I2S_DOUT
  );

  float initialGain =
    (float)volume /
    (float)MAX_VOLUME;

  out->SetGain(
    initialGain
  );

  Serial.println(
    "I2S ready."
  );

  Serial.print(
    "Initial volume: "
  );

  Serial.println(
    volume
  );


  // ==========================================================
  // NFC MUTEX
  // ==========================================================

  nfcMutex =
    xSemaphoreCreateMutex();

  if (nfcMutex == nullptr)
  {
    Serial.println(
      "ERROR: Failed to create NFC mutex!"
    );

    while (true)
    {
      delay(1000);
    }
  }


  // ==========================================================
  // START NFC TASK ON CORE 0
  // ==========================================================

  Serial.println();
  Serial.println(
    "Starting NFC task on Core 0..."
  );

  BaseType_t taskResult =
    xTaskCreatePinnedToCore(
      nfcTask,
      "NFC_Task",
      4096,
      nullptr,
      1,
      &nfcTaskHandle,
      0
    );

  if (
    taskResult != pdPASS
  )
  {
    Serial.println(
      "ERROR: Failed to create NFC task!"
    );

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "NFC task started."
  );

  Serial.println();
  Serial.println(
    "======================================"
  );

  Serial.println(
    "SYSTEM READY"
  );

  Serial.println(
    "Place NFC tag on reader."
  );

  Serial.println(
    "======================================"
  );
}


// ============================================================
// MAIN LOOP
//
// Arduino loop normally runs on Core 1
// ============================================================

void loop()
{
  // ==========================================================
  // AUDIO LOOP
  // ==========================================================

  if (
    isPlaying &&
    mp3 != nullptr
  )
  {
    if (
      mp3->isRunning()
    )
    {
      if (
        !mp3->loop()
      )
      {
        Serial.println();
        Serial.println(
          "MP3 playback finished."
        );

        mp3->stop();

        isPlaying = false;


        // ----------------------------------------------------
        // Track finished
        // ----------------------------------------------------

        loopCount++;

        Serial.print(
          "Loop "
        );

        Serial.print(
          loopCount
        );

        Serial.print(
          " / "
        );

        Serial.println(
          MAX_LOOPS
        );


        if (
          loopCount < MAX_LOOPS
        )
        {
          // --------------------------------------------------
          // LED loop indication
          // --------------------------------------------------

          setLEDMode(
            LED_LOOP
          );


          // --------------------------------------------------
          // Replay same track
          // --------------------------------------------------

          char replayFile[40];

          strncpy(
            replayFile,
            playingFile,
            sizeof(replayFile) - 1
          );

          replayFile[
            sizeof(replayFile) - 1
          ] = '\0';

          playTrack(
            replayFile
          );
        }
        else
        {
          // --------------------------------------------------
          // All loops completed
          // --------------------------------------------------

          stopPlayback();

          enterDeepSleep();
        }
      }
    }
  }


  // ==========================================================
  // NFC: NEW TAG
  // ==========================================================

  bool newTag = false;

  if (
    nfcMutex != nullptr
  )
  {
    xSemaphoreTake(
      nfcMutex,
      portMAX_DELAY
    );
  }

  if (
    nfcTagDetected
  )
  {
    nfcTagDetected = false;

    newTag = true;
  }

  char detectedUID[15] = "";

  if (newTag)
  {
    strncpy(
      detectedUID,
      nfcUID,
      sizeof(detectedUID) - 1
    );

    detectedUID[
      sizeof(detectedUID) - 1
    ] = '\0';
  }

  if (
    nfcMutex != nullptr
  )
  {
    xSemaphoreGive(
      nfcMutex
    );
  }


  // ==========================================================
  // PROCESS NEW TAG
  // ==========================================================

  if (newTag)
  {
    Serial.println();

    Serial.print(
      "Processing NFC UID: "
    );

    Serial.println(
      detectedUID
    );


    // Save current UID
    strncpy(
      currentUID,
      detectedUID,
      sizeof(currentUID) - 1
    );

    currentUID[
      sizeof(currentUID) - 1
    ] = '\0';


    // Build filename
    //
    // IMPORTANT:
    // Keep the original working path:
    //
    // /player/<UID>.mp3

    char filename[40];

    snprintf(
      filename,
      sizeof(filename),
      "/player/%s.mp3",
      currentUID
    );


    Serial.print(
      "Mapped file: "
    );

    Serial.println(
      filename
    );


    // Reset loop counter
    loopCount = 0;


    // Start track
    if (
      playTrack(
        filename
      )
    )
    {
      // Playback has started.
      // The starting LED animation will
      // transition naturally into playing.
    }
    else
    {
      Serial.println(
        "Could not start track."
      );

      currentUID[0] = '\0';

      setLEDMode(
        LED_STANDBY
      );
    }
  }


  // ==========================================================
  // NFC: TAG REMOVED
  // ==========================================================

  bool tagRemoved = false;

  if (
    nfcMutex != nullptr
  )
  {
    xSemaphoreTake(
      nfcMutex,
      portMAX_DELAY
    );
  }

  if (
    nfcTagRemoved
  )
  {
    nfcTagRemoved = false;

    tagRemoved = true;
  }

  if (
    nfcMutex != nullptr
  )
  {
    xSemaphoreGive(
      nfcMutex
    );
  }


  if (tagRemoved)
  {
    Serial.println(
      "Tag removal detected."
    );


    // --------------------------------------------------------
    // LED stopping animation
    // --------------------------------------------------------

    setLEDMode(
      LED_STOPPING
    );


    // Clear current UID
    currentUID[0] = '\0';


    // Stop current playback
    if (isPlaying)
    {
      stopPlayback();
    }
  }


  // ==========================================================
  // ENCODER
  // ==========================================================

  handleEncoder();


  // ==========================================================
  // LED UPDATE
  // ==========================================================

  updateLEDs();


  // ==========================================================
  // VERY SHORT DELAY
  // ==========================================================

  delay(1);
}