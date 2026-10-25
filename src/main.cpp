#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c.h"
#include "oled.h"
#include "ds1307.h"

static const char *TAG = "app";

// Піни та порт шини I2C
#define I2C_MASTER_SCL_IO  GPIO_NUM_9
#define I2C_MASTER_SDA_IO  GPIO_NUM_8
#define I2C_MASTER_NUM     I2C_NUM_0

// Назви днів тижня для DS1307: 1 = неділя, ..., 7 = субота
static const char *const WEEKDAYS[] = {
    "", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

// День тижня за датою (алгоритм Сакамото): 1 = неділя, ..., 7 = субота
static uint8_t weekday_from_date(int year, int month, int day)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year -= 1;
    }
    return (uint8_t)((year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7 + 1);
}

// Дата й час компіляції прошивки з макросів __DATE__ ("Oct 25 2026") і __TIME__ ("16:54:42")
static ds1307_datetime_t build_datetime(void)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char month_name[4] = {};
    int day = 1, year = 2000, hour = 0, minute = 0, second = 0;
    sscanf(__DATE__, "%3s %d %d", month_name, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    const int month = (int)(strstr(months, month_name) - months) / 3 + 1;

    ds1307_datetime_t dt = {};
    dt.year = (uint16_t)year;
    dt.month = (uint8_t)month;
    dt.day = (uint8_t)day;
    dt.weekday = weekday_from_date(year, month, day);
    dt.hour = (uint8_t)hour;
    dt.minute = (uint8_t)minute;
    dt.second = (uint8_t)second;
    return dt;
}

extern "C" void app_main()
{
    vTaskDelay(pdMS_TO_TICKS(2000));   // чекаємо, поки підключиться монітор
    ESP_LOGI(TAG, "Start");

    // 1. Створюємо шину I2C
    i2c_master_bus_handle_t bus = NULL;
    i2c_bus_settings_t bus_cfg = {};
    bus_cfg.port = I2C_MASTER_NUM;
    bus_cfg.sda_pin = I2C_MASTER_SDA_IO;
    bus_cfg.scl_pin = I2C_MASTER_SCL_IO;
    bus_cfg.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_bus_init(&bus_cfg, &bus));

    // 2. Ініціалізуємо дисплей 128x64 (адреса 0x3C, 400 кГц за замовчуванням)
    oled_settings_t oled_cfg = OLED_SETTINGS_128X64_DEFAULT();
    oled_handle_t oled = NULL;
    esp_err_t err = oled_init(bus, &oled_cfg, &oled);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_ERROR_CHECK(oled_clear(oled));

    // 3. Ініціалізуємо годинник DS1307 (адреса 0x68)
    ds1307_handle_t rtc = NULL;
    err = ds1307_init(bus, &rtc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DS1307 init failed: %s", esp_err_to_name(err));
        oled_print(oled, 3, 1, false, "RTC init error");
        return;
    }

    // Якщо генератор зупинений (новий модуль або сіла батарейка) -
    // записуємо час компіляції прошивки. Інакше час іде з батарейки, не чіпаємо його.
    bool running = false;
    err = ds1307_is_running(rtc, &running);
    if (err == ESP_OK && !running) {
        const ds1307_datetime_t dt = build_datetime();
        ESP_LOGW(TAG, "RTC stopped, setting build time %04d-%02d-%02d %02d:%02d:%02d",
                 dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        err = ds1307_set_datetime(rtc, &dt);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "DS1307 set datetime failed: %s", esp_err_to_name(err));
        }
    }

    // 4. Читаємо годинник 5 разів на секунду і перемальовуємо OLED, коли зміниться секунда
    //    рядок 0     - день тижня (шрифт 8x8)
    //    рядки 2-3   - дата ДД.ММ.РР (шрифт 16x16)
    //    рядки 5-6   - час ГГ:ХХ:СС (шрифт 16x16)
    uint8_t last_second = 0xFF;
    while (1) {
        ds1307_datetime_t now;
        err = ds1307_get_datetime(rtc, &now);
        if (err == ESP_OK && now.second == last_second) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (err == ESP_OK) {
            last_second = now.second;
            ESP_LOGI(TAG, "%04d-%02d-%02d %02d:%02d:%02d",
                     now.year, now.month, now.day, now.hour, now.minute, now.second);
            oled_printf(oled, 0, 1, "%-16s", WEEKDAYS[now.weekday]);
            oled_printf(oled, 2, 2, "%02d.%02d.%02d", now.day, now.month, now.year % 100);
            oled_printf(oled, 5, 2, "%02d:%02d:%02d", now.hour, now.minute, now.second);
        } else {
            ESP_LOGE(TAG, "DS1307 get datetime failed: %s", esp_err_to_name(err));
            oled_printf(oled, 0, 1, "%-16s", "RTC read error");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
