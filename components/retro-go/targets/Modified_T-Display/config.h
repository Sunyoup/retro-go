// Target definition
// LilyGO TTGO T-Display (ESP32-D0WDQ6, 16MB flash, ST7789 135x240 IPS) modified:
// - 8MB PSRAM hand-soldered on top of the flash (CS = GPIO16, CLK = GPIO17)
// - LCD DC moved from GPIO16 to GPIO13 (GPIO16 is now PSRAM CS)
// - SPI SD card on GPIO2/15/22/26, buttons and PAM8302A amplifier on a breadboard
#define RG_TARGET_NAME             "MODIFIED-T-DISPLAY"

// Storage
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDSPI_HOST       SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT
// #define RG_STORAGE_SDMMC_HOST       SDMMC_HOST_SLOT_1
// #define RG_STORAGE_SDMMC_SPEED      SDMMC_FREQ_DEFAULT
// #define RG_STORAGE_FLASH_PARTITION  "vfs"

// Audio
#define RG_AUDIO_USE_INT_DAC        1   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        0   // 0 = Disable, 1 = Enable

// Video
// The ST7789 has 240x320 of RAM, the 135x240 panel is a window in it. In landscape
// (MADCTL MY|MV) the visible area starts at x=40, y=53 (same as Arduino_GFX rotation 3).
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341/ST7789
#define RG_SCREEN_HOST              SPI3_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {40, 53, 40, 52} // Left, Top, Right, Bottom
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_INIT()                                                                                   \
    ILI9341_CMD(0x36, 0xA8);                 /* Memory Access Control (MY|MV|BGR) */                      \
    ILI9341_CMD(0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33); /* Porch Setting */                                  \
    ILI9341_CMD(0xB7, 0x35);                 /* Gate Control */                                           \
    ILI9341_CMD(0xBB, 0x28);                 /* VCOM Setting */                                           \
    ILI9341_CMD(0xC0, 0x0C);                 /* LCM Control */                                            \
    ILI9341_CMD(0xC2, 0x01, 0xFF);           /* VDV and VRH Command Enable */                             \
    ILI9341_CMD(0xC3, 0x10);                 /* VRH Set */                                                \
    ILI9341_CMD(0xC4, 0x20);                 /* VDV Set */                                                \
    ILI9341_CMD(0xC6, 0x0F);                 /* Frame Rate Control in Normal Mode (60Hz) */               \
    ILI9341_CMD(0xD0, 0xA4, 0xA1);           /* Power Control 1 */                                        \
    ILI9341_CMD(0xE0, 0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0E, 0x12, 0x14, 0x17); \
    ILI9341_CMD(0xE1, 0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54, 0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E); \
    ILI9341_CMD(0x21);                       /* Display Inversion On (IPS panel) */

// Input
// Refer to rg_input.h to see all available RG_KEY_* and RG_GAMEPAD_*_MAP types
// UP/DOWN share GPIO38 and LEFT/RIGHT share GPIO37 (10k pull-down on each pin):
// UP/LEFT connect 3V3 directly (~4095), DOWN/RIGHT go through a 10k resistor (~2048).
#define RG_GAMEPAD_ADC_MAP {\
    {RG_KEY_UP,    ADC_UNIT_1, ADC_CHANNEL_2, ADC_ATTEN_DB_11, 3072, 4096},\
    {RG_KEY_DOWN,  ADC_UNIT_1, ADC_CHANNEL_2, ADC_ATTEN_DB_11, 1024, 3071},\
    {RG_KEY_LEFT,  ADC_UNIT_1, ADC_CHANNEL_1, ADC_ATTEN_DB_11, 3072, 4096},\
    {RG_KEY_RIGHT, ADC_UNIT_1, ADC_CHANNEL_1, ADC_ATTEN_DB_11, 1024, 3071},\
}
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_SELECT, .num = GPIO_NUM_27, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_39, .pullup = 0, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_32, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_33, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_35, .pullup = 0, .level = 0},\
    {RG_KEY_OPTION, .num = GPIO_NUM_0,  .pullup = 0, .level = 0},\
}
// The on-board MENU/OPTION buttons are hard to reach in the enclosure
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_MENU,   .src = RG_KEY_START | RG_KEY_SELECT},\
}

// There are no spare buttons for MENU/OPTION in the enclosure: power cycle to get back to the launcher
#define RG_BOOT_LAUNCHER_ON_POWER_ON 1

// Battery
// On-board divider (100k + 100k): BAT -> 1/2 -> GPIO34 (ADC1_CH6)
// raw is the calibrated voltage at the pin in mV, so the battery voltage is raw * 2.
// On newer T-Display boards the divider is only connected on battery power while ADC_EN (GPIO14) is high.
#define RG_BATTERY_DRIVER           1
#define RG_GPIO_BATTERY_ADC_ENABLE  GPIO_NUM_14
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_6
#define RG_BATTERY_CALC_PERCENT(raw) (((raw) * 2.f - 3500.f) / (4200.f - 3500.f) * 100.f)
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 2.f * 0.001f)

// Status LED
// #define RG_GPIO_LED                 GPIO_NUM_NC

// SPI Display
#define RG_GPIO_LCD_MISO            GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI            GPIO_NUM_19
#define RG_GPIO_LCD_CLK             GPIO_NUM_18
#define RG_GPIO_LCD_CS              GPIO_NUM_5
#define RG_GPIO_LCD_DC              GPIO_NUM_13
#define RG_GPIO_LCD_BCKL            GPIO_NUM_4
#define RG_GPIO_LCD_RST             GPIO_NUM_23

// SPI SD Card
// MISO moved from GPIO12 to GPIO22:
// - GPIO12 is a strapping pin (MTDI), the SD module pulling it high at reset selects 1.8V flash voltage.
// - GPIO36/39 get ~80ns low glitches whenever the SAR ADC powers up (ESP32 errata), which happens on
//   every ADC gamepad read and corrupted SD reads ("ROM: Read error").
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_22
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_15
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_2
#define RG_GPIO_SDSPI_CS            GPIO_NUM_26
