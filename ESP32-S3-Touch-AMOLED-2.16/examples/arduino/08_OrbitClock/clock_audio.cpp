#include "clock_audio.h"
#include "clock_audio_model.h"
#include "pikachu_pcm.h"
#include "es8311.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include <pin_config.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace clock_audio {
namespace {
SingleFlight flight;
i2s_chan_handle_t tx=nullptr;
es8311_handle_t codec=nullptr;
TaskHandle_t worker=nullptr;
bool channelEnabled=false;
std::atomic<int> error{ESP_OK};
std::atomic<uint32_t> plays{0};

bool initFailed(esp_err_t cause) {
  digitalWrite(PA,LOW);
  if(tx) {
    if(channelEnabled)i2s_channel_disable(tx);
    i2s_del_channel(tx);tx=nullptr;channelEnabled=false;
  }
  if(codec) { es8311_delete(codec);codec=nullptr; }
  error.store(cause);flight.fail();return false;
}

struct I2sSink {
  esp_err_t result=ESP_OK;
  bool writeSome(const void *data,size_t bytes,size_t &written) {
    result=i2s_channel_write(tx,data,bytes,&written,500);
    return result==ESP_OK;
  }
};

void audioTask(void *) {
  for(;;) {
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    digitalWrite(PA,HIGH);
    I2sSink sink;
    const bool ok=streamEffect(pikachu_pcm,PIKACHU_SAMPLE_COUNT,sink);
    digitalWrite(PA,LOW);
    if(ok) {
      // streamEffect has already flushed the final audio samples through DMA.
      flight.finish();
    } else {
      // Stop faulty output; keep the clock usable and reject further playback.
      i2s_channel_disable(tx);channelEnabled=false;
      error.store(sink.result==ESP_OK?ESP_FAIL:sink.result);flight.fail();
    }
  }
}
}

bool begin() {
  if(flight.state()!=State::Unavailable)return flight.state()!=State::Failed;
  pinMode(PA,OUTPUT);digitalWrite(PA,LOW);
  // Wire is shared with touch/IMU. Reusing the same initialized bus does not reset it.
  if(!Wire.begin(IIC_SDA,IIC_SCL))return initFailed(ESP_FAIL);
  i2s_chan_config_t channel=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO,I2S_ROLE_MASTER);
  channel.dma_desc_num=DMA_BUFFERS;channel.dma_frame_num=DMA_FRAMES;
  channel.auto_clear_after_cb=true; // Never loop old PCM on an underrun or while idle.
  esp_err_t result=i2s_new_channel(&channel,&tx,nullptr);
  if(result!=ESP_OK)return initFailed(result);
  i2s_std_config_t standard={
    .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(PIKACHU_SAMPLE_RATE),
    .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
    .gpio_cfg={
      .mclk=(gpio_num_t)PIN_ES7210_MCLK,.bclk=(gpio_num_t)PIN_ES7210_BCLK,
      .ws=(gpio_num_t)PIN_ES7210_LRCK,.dout=(gpio_num_t)PIN_ES8311_DOUT,
      .din=I2S_GPIO_UNUSED,.invert_flags={.mclk_inv=false,.bclk_inv=false,.ws_inv=false}
    }
  };
  result=i2s_channel_init_std_mode(tx,&standard);
  if(result!=ESP_OK)return initFailed(result);
  result=i2s_channel_enable(tx);
  if(result!=ESP_OK)return initFailed(result);
  channelEnabled=true;
  // Same codec address, MCLK ratio and 16-bit I2S format as official 07_ES8311.
  codec=es8311_create(0,ES8311_ADDRESS_0);
  if(!codec)return initFailed(ESP_ERR_NO_MEM);
  const es8311_clock_config_t clocks={false,false,true,PIKACHU_SAMPLE_RATE*256,PIKACHU_SAMPLE_RATE};
  result=es8311_init(codec,&clocks,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16);
  if(result==ESP_OK)result=es8311_sample_frequency_config(codec,clocks.mclk_frequency,clocks.sample_frequency);
  if(result==ESP_OK)result=es8311_voice_volume_set(codec,65,nullptr);
  if(result==ESP_OK)result=es8311_voice_mute(codec,false);
  if(result!=ESP_OK)return initFailed(result);
  if(xTaskCreatePinnedToCore(audioTask,"pika_sound",4096,nullptr,2,&worker,0)!=pdPASS)
    return initFailed(ESP_ERR_NO_MEM);
  flight.enable();return true;
}

bool play() {
  if(!flight.request())return false;
  plays.fetch_add(1);xTaskNotifyGive(worker);return true;
}

const char *status() {
  switch(flight.state()) {
    case State::Idle:return "READY";
    case State::Playing:return "PLAYING";
    case State::Failed:return "FAILED";
    default:return "UNAVAILABLE";
  }
}
int lastError() { return error.load(); }
uint32_t playCount() { return plays.load(); }
}
