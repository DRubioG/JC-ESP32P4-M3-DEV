#include "string.h"
#include "AudioBoard.h"  //https://github.com/pschatzmann/arduino-audio-driver
#include "Audio.h"       //https://github.com/schreibfaul1/ESP32-audioI2S
#include "SD_MMC.h"

Audio audio;
DriverPins my_pins;
AudioBoard board(AudioDriverES8311, my_pins);  

//SD_MMC 
#define SD_D0    39
#define SD_D1    40
#define SD_D2    41
#define SD_D3    42
#define SD_CMD   44
#define SD_CLK   43

#define I2S_MCK_IO 13
#define I2S_BCK_IO 12
#define I2S_DI_IO 48
#define I2S_WS_IO 10  
#define I2S_DO_IO 9
#define ES8311_PA 11

#define I2C_SDA 7  
#define I2C_SCL 8 
#define ES8311_ADDRESS 0x18

static char file_list[20][256];
static int file_cnt = 0;
static int cnt = 0;


void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      snprintf(&file_list[file_cnt][0],256,"/music/%s",file.name());
      file_cnt++;
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void setup() {

  Serial.begin(115200);
  Serial.println("start");
  if(! SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)){
       Serial.println("Pin change failed!");
       return;
    }
  
  if (!SD_MMC.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  listDir(SD_MMC,"/music/",0);

  // pinMode(ES8311_PA, OUTPUT);
  // digitalWrite(ES8311_PA, HIGH);

  my_pins.addI2C(PinFunction::CODEC,I2C_SCL,I2C_SDA,ES8311_ADDRESS);
  my_pins.addPin(PinFunction::PA,ES8311_PA,PinLogic::Output);

  CodecConfig cfg;
  cfg.input_device = ADC_INPUT_LINE1;
  cfg.output_device = DAC_OUTPUT_ALL;
  cfg.i2s.bits = BIT_LENGTH_16BITS;
  cfg.i2s.rate = RATE_44K;
  // cfg.i2s.fmt = I2S_NORMAL;
  // cfg.i2s.mode = MODE_SLAVE;
  board.begin(cfg);

  audio.setPinout(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO,I2S_MCK_IO);
  audio.setVolume(4); // 0...21
  audio.connecttoFS(SD_MMC, file_list[cnt]);
}

void loop() {
audio.loop();
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
    cnt++;
    if(cnt > file_cnt - 1)
      cnt = 0;

    audio.connecttoFS(SD_MMC, file_list[cnt]);

}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}