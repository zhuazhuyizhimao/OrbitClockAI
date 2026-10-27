// Included at the end of generated orbit_ui.cc, sharing its maintained UI state.
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include "orbit_time_sync.h"
#include "orbit_touch_calibration_ui.h"
#include "orbit_touch_store.h"
static bool voiceWasVisible = false;
static uint32_t lastUi = 0, lastPixel = 0, lastClock = 0;
static orbit_clock_ai::NetworkClock networkClock;
static lv_obj_t* networkNoticeLabel = nullptr;
static uint32_t networkNoticeAt = 0;
static bool key3WasDown = false, key3HoldUsed = false;
static bool calibrationOffered = false;
static uint32_t key3PressedAt = 0;
namespace orbit_clock_ai {
void InitializeUi() {
    setenv("TZ", CLOCK_TIMEZONE, 1);
    tzset();
    gpio_config_t button = {};
    button.pin_bit_mask = 1ULL << USER_BUTTON_PIN;
    button.mode = GPIO_MODE_INPUT;
    button.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&button);
    userButton.begin(gpio_get_level((gpio_num_t)USER_BUTTON_PIN) == 0, millis());
    diceRandomSource = esp_random;
    storeDiceSettings = saveDiceSettings;
    storeSandSeconds = saveSandSeconds;
    storePlayerCount = savePlayerCount;
    storeOrientationSettings = saveOrientationSettings;
    loadDiceSettings();
    loadSandSeconds();
    loadPlayerCount();
    loadOrientationSettings();
    motionOnline = motionSensor.begin(I2cBus(), QMI8658_L_SLAVE_ADDRESS);
    if (motionOnline) {
        motionOnline = motionSensor.configAccelerometer(SensorQMI8658::ACC_RANGE_8G,
                                                        SensorQMI8658::ACC_ODR_125Hz,
                                                        SensorQMI8658::LPF_MODE_3) == 0 &&
                       motionSensor.enableAccelerometer();
        motionGyroOnline = motionSensor.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS,
                                                        SensorQMI8658::GYR_ODR_112_1Hz,
                                                        SensorQMI8658::LPF_MODE_0) == 0 &&
                           motionSensor.enableGyroscope();
    }
    ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    buildUi();
    refreshUi(nullptr);
    SetRotation(pageRotation);
    orbit_touch_calibration::Configure(
        +[]() {
            return orbit_touch::Sample{orbit_touch::raw_pressed, orbit_touch::raw_x,
                                       orbit_touch::raw_y};
        },
        orbit_touch::SaveCalibration);
    networkNoticeLabel = lv_label_create(lv_screen_active());
    lv_obj_set_width(networkNoticeLabel, 380);
    lv_label_set_long_mode(networkNoticeLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(networkNoticeLabel, &orbit_font_ai_20, 0);
    lv_obj_set_style_text_align(networkNoticeLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(networkNoticeLabel, lv_color_hex(0xF0CF72), 0);
    lv_obj_set_style_bg_color(networkNoticeLabel, lv_color_hex(0x111C2B), 0);
    lv_obj_set_style_bg_opa(networkNoticeLabel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(networkNoticeLabel, 12, 0);
    lv_obj_set_style_radius(networkNoticeLabel, 12, 0);
    lv_obj_add_flag(networkNoticeLabel, LV_OBJ_FLAG_HIDDEN);
    clock_audio::begin();
    USBSerial.printf("ORBIT READY: display=480x480 imu=%s pixel_clock=1 ai=1\n",
                     motionOnline ? "OK" : "FAILED");
}
void SetVoiceVisible(bool show) {
    if (show && !voiceWasVisible) {
        orbit_touch_calibration::Cancel();
        key3WasDown = key3HoldUsed = false;
        sandGesture.Cancel();
        diceClicks.reset();
        diceGame.settings(true);
        sandTimer.pause(millis());
        playerGame.cancel(millis());
        pageOrientation.hold(pageRotation, millis());
    } else if (!show && voiceWasVisible) {
        diceGame.settings(diceSettingsPanel || sandMode);
        if (playerMode) {
            rebuildPlayerFrame();
            updatePlayerLabels();
        } else if (sandMode) {
            rebuildSandFrame();
            updateSandLabels();
        } else if (diceMode) {
            rebuildDiceFrame();
            updateDiceLabels();
        }
    }
    visible(screen, !show);
    voiceWasVisible = show;
}
void PollUi(bool voice_visible) {
    for (lv_indev_t* input = lv_indev_get_next(nullptr); input; input = lv_indev_get_next(input))
        if (lv_indev_get_type(input) == LV_INDEV_TYPE_POINTER)
            touchDown = lv_indev_get_state(input) == LV_INDEV_STATE_PRESSED;
    serviceConsole();
    const auto timeNotice =
        networkClock.Poll(WiFi.status() == WL_CONNECTED, millis(), TakeClockSyncRequest());
    auto notice = TakeNetworkNotice();
    if (timeNotice != NetworkNotice::None)
        notice = timeNotice;
    if (timeNotice == NetworkNotice::Synced) {
        ntpStarted = true;
        USBSerial.println("ORBIT NETWORK TIME SYNCED (UTC; display CST-8)");
    }
    if (notice != NetworkNotice::None) {
        lv_label_set_text(networkNoticeLabel, NetworkNoticeText(notice));
        lv_obj_align(networkNoticeLabel, LV_ALIGN_TOP_MID, 0, 82);
        lv_obj_remove_flag(networkNoticeLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(networkNoticeLabel);
        networkNoticeAt = millis();
    } else if (uint32_t(millis() - networkNoticeAt) >= 3500) {
        lv_obj_add_flag(networkNoticeLabel, LV_OBJ_FLAG_HIDDEN);
    }
    const bool down = gpio_get_level((gpio_num_t)USER_BUTTON_PIN) == 0;
    if (voice_visible) {
        if (userButton.update(down, millis()))
            EndConversation();
        updateRemaining();
        return;
    }
    const auto now = millis();
    if (down && !key3WasDown) {
        key3PressedAt = now;
        key3HoldUsed = false;
    }
    key3WasDown = down;
    if (orbit_touch_calibration::Active()) {
        if (userButton.update(down, now))
            orbit_touch_calibration::Cancel();
        orbit_touch_calibration::Poll(now);
        updateRemaining();
        if (!orbit_touch_calibration::Active())
            diceGame.settings(diceSettingsPanel || sandMode);
        return;
    }
    const bool manualCalibration = down && !key3HoldUsed && uint32_t(now - key3PressedAt) >= 3000;
    const bool firstSandCalibration = sandMode && !playerMode && !sandSettingsPanel &&
                                      !orbit_touch::mapping.Valid() && !calibrationOffered;
    if (manualCalibration || firstSandCalibration) {
        if (manualCalibration)
            key3HoldUsed = true;
        calibrationOffered = true;
        diceClicks.reset();
        diceGame.settings(true);
        sandTimer.pause(now);
        playerGame.cancel(now);
        sandGesture.Cancel();
        pageOrientation.hold(pageRotation, now);
        orbit_touch_calibration::Open();
        return;
    }
    handleUserButton(down, millis());
    serviceMotion();
    if (uint32_t(now - lastUi) >= 40) {
        lastUi = now;
        diceAnimationTick(nullptr);
    }
    if (uint32_t(now - lastPixel) >= 60) {
        lastPixel = now;
        pixelAnimationTick(nullptr);
    }
    if (uint32_t(now - lastClock) >= 100) {
        lastClock = now;
        refreshUi(nullptr);
    }
}
}  // namespace orbit_clock_ai
