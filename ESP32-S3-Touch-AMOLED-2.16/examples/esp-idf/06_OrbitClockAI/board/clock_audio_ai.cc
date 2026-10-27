#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cstring>
#include "application.h"
#include "clock_audio.h"
#include "clock_audio_model.h"
#include "ogg_demuxer.h"
#include "pikachu_ogg.inc"

namespace clock_audio {
namespace {
SingleFlight flight;
TaskHandle_t worker = nullptr;
std::atomic<uint32_t> plays{0};
std::atomic<int> error{ESP_OK};
bool canPlay() {
    const auto state = Application::GetInstance().GetDeviceState();
    return state == kDeviceStateIdle || state == kDeviceStateWifiConfiguring ||
           state == kDeviceStateActivating;
}
void effectTask(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        auto& audio = Application::GetInstance().GetAudioService();
        if (!canPlay() || !audio.IsPlaybackIdle()) {
            flight.finish();
            continue;
        }
        const bool restore_wake = audio.IsWakeWordRunning();
        if (restore_wake)
            audio.EnableWakeWordDetection(false);
        const auto initial_state = Application::GetInstance().GetDeviceState();
        const int64_t feed_deadline = esp_timer_get_time() + 4000000;
        bool feed_failed = false;
        error.store(ESP_OK);
        OggDemuxer demuxer;
        demuxer.OnPacket([&audio, initial_state, feed_deadline, &feed_failed](
                             const uint8_t* data, int sample_rate, int duration, size_t size) {
            if (feed_failed || !canPlay() ||
                Application::GetInstance().GetDeviceState() != initial_state)
                return;
            auto packet = std::make_unique<AudioStreamPacket>();
            packet->sample_rate = sample_rate;
            packet->frame_duration = duration;
            packet->payload.resize(size);
            std::memcpy(packet->payload.data(), data, size);
            // Bound the producer's wait. The UI, AI event loop and audio tasks never
            // wait for this worker, and repeated taps cannot enqueue another effect.
            const int64_t deadline = std::min(feed_deadline, esp_timer_get_time() + 500000);
            while (canPlay() && esp_timer_get_time() < deadline) {
                // PushPacket consumes its argument even when the queue is full. Retain
                // one packet and offer a copy on each bounded, nonblocking attempt.
                auto copy = std::make_unique<AudioStreamPacket>(*packet);
                if (audio.PushPacketToDecodeQueue(std::move(copy), false))
                    return;
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            feed_failed = true;
            if (canPlay())
                error.store(ESP_ERR_TIMEOUT);
        });
        demuxer.Reset();
        demuxer.Process(effect_ogg, sizeof(effect_ogg));
        const int64_t deadline = esp_timer_get_time() + 4000000;
        while (canPlay() && !audio.IsPlaybackIdle() && esp_timer_get_time() < deadline)
            vTaskDelay(pdMS_TO_TICKS(20));
        if (restore_wake && Application::GetInstance().GetDeviceState() == kDeviceStateIdle)
            audio.EnableWakeWordDetection(true);
        flight.finish();
    }
}
}  // namespace
bool begin() {
    if (worker)
        return true;
    if (xTaskCreatePinnedToCore(effectTask, "orbit_effect", 8192, nullptr, 2, &worker, 0) !=
        pdPASS) {
        error.store(ESP_ERR_NO_MEM);
        flight.fail();
        return false;
    }
    flight.enable();
    return true;
}
bool play() {
    if (!canPlay() || !worker || !flight.request())
        return false;
    plays.fetch_add(1);
    xTaskNotifyGive(worker);
    return true;
}
const char* status() {
    switch (flight.state()) {
        case State::Idle:
            return "READY";
        case State::Playing:
            return "PLAYING";
        case State::Failed:
            return "FAILED";
        default:
            return "UNAVAILABLE";
    }
}
int lastError() { return error.load(); }
uint32_t playCount() { return plays.load(); }
}  // namespace clock_audio
