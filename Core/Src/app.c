/*
 * app.c
 *
 *  Created on: Aug 23, 2026
 *      Author: Karthik
 */
#include "app.h"
#include <stdio.h>
#include "ssd1306.h"
#include "BMP180.h"
#include "WS2812_SPI.h"
#include "ESP8266_STM32.h"

extern ADC_HandleTypeDef hadc1;

#define WIFI_SSID "KarthikT"
#define WIFI_PASS "deathnote"

static char esp_ip[32];
#define THINGSPEAK_API_KEY "LXSUW3A87OX1RND3"
static uint32_t publish_counter = 0;
int brightness = 40;
extern UART_HandleTypeDef huart2;
static char ts_status_line[16] = "TS:--";
static uint32_t counter = 0;
extern I2C_HandleTypeDef hi2c1;
static volatile uint8_t manual_publish_requested = 0;

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
    return len;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_0)
    {
        manual_publish_requested = 1;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        HAL_GPIO_TogglePin(GPIOD, LD3_Pin);
    }
}

void App_Init(void)
{
    printf("STM32 IoT Environment Monitor booting...\r\n");
    printf("Scanning I2C bus...\r\n");
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 2, 10) == HAL_OK) {
            printf("Found device at 0x%02X\r\n", addr);
        }
    }
    if (SSD1306_Init() == 0) {
        printf("OLED NOT DETECTED on I2C bus!\r\n");
    }
    BMP180_Start();
    SSD1306_Fill(SSD1306_COLOR_BLACK);
    SSD1306_GotoXY(2, 0);
    SSD1306_Puts("IoT Env Monitor", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 20);
    SSD1306_Puts("Booting...", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen();

    for (int i = 0; i < 16; i++) setLED(i, 0, 0, 255); // all blue: booting
    WS2812_Send();
    if (ESP_Init() == ESP8266_OK)
    {
        ESP_ConnectWiFi(WIFI_SSID, WIFI_PASS, esp_ip, sizeof(esp_ip));
    }
}

void App_Run(void)
{
    float temp = BMP180_GetTemp();
    float press = BMP180_GetPress(0);

    printf("STM32 IoT Environment Monitor alive, tick %lu | Temp: %.1f C | Press: %.1f hPa\r\n",
           (unsigned long)counter++, temp, press / 100.0f);


    char buf[20];
    sprintf(buf, "T:%.1fC P:%.0fhPa", temp, press / 100.0f);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_raw = HAL_ADC_GetValue(&hadc1);
    float mcu_temp = ((adc_raw * 3.3f / 4095.0f) - 0.76f) / 0.0025f + 25.0f;
    printf("MCU Temp: %.1f C\r\n", mcu_temp);

    char mcu_line[24];
    snprintf(mcu_line, sizeof(mcu_line), "MCU:%.1fC", mcu_temp);

    char wifi_line[24];
    if (ESP_ConnState == ESP8266_CONNECTED_IP)
        snprintf(wifi_line, sizeof(wifi_line), "IP:%s", esp_ip);
    else
        snprintf(wifi_line, sizeof(wifi_line), "WiFi:OFF");

    SSD1306_Fill(SSD1306_COLOR_BLACK);
    SSD1306_GotoXY(2, 0);
    SSD1306_Puts("IoT Env Monitor", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 13);
    SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 26);
    SSD1306_Puts(mcu_line, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 39);
    SSD1306_Puts(wifi_line, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(2, 52);
    SSD1306_Puts(ts_status_line, &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen();
    if (manual_publish_requested)
    {
        manual_publish_requested = 0;
        printf("Manual publish triggered by button!\r\n");
        if (ESP_ConnState == ESP8266_CONNECTED_IP)
        {
            ESP8266_Status pub_result = ESP_SendToThingSpeak(THINGSPEAK_API_KEY, temp, press / 100.0f, mcu_temp);
            if (pub_result == ESP8266_OK) {
                for (int i = 0; i < 16; i++) setLED(i, 0, 255, 0);
                snprintf(ts_status_line, sizeof(ts_status_line), "TS:OK");
            } else {
                for (int i = 0; i < 16; i++) setLED(i, 255, 0, 0);
                snprintf(ts_status_line, sizeof(ts_status_line), "TS:FAIL");
            }
            WS2812_Send();
        }
    }
    if (ESP_ConnState == ESP8266_CONNECTED_IP)
    {
        publish_counter++;
        if (publish_counter >= 3)
        {
            publish_counter = 0;
            ESP8266_Status pub_result = ESP_SendToThingSpeak(THINGSPEAK_API_KEY, temp, press / 100.0f, mcu_temp);
            if (pub_result == ESP8266_OK) {
                for (int i = 0; i < 16; i++) setLED(i, 0, 255, 0);
                snprintf(ts_status_line, sizeof(ts_status_line), "TS:OK");
            } else {
                for (int i = 0; i < 16; i++) setLED(i, 255, 0, 0);
                snprintf(ts_status_line, sizeof(ts_status_line), "TS:FAIL");
            }
            WS2812_Send();
        }
    }
    else
    {
        for (int i = 0; i < 16; i++) setLED(i, 255, 0, 0); // red: WiFi disconnected
        WS2812_Send();
    }

    HAL_GPIO_TogglePin(GPIOD, LD4_Pin);
    HAL_Delay(5000);
}

