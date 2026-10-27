#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "gif/lvgl_gif.h"

int main(int argc, char** argv) {
    assert(argc == 3);
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    assert(bytes.size() > 13);
    lv_init();
    auto* gif = gd_open_gif_data(bytes.data());
    assert(gif && gif->width == 400 && gif->height == 180);
    auto* decoded = std::fopen(argv[2], "wb");
    assert(decoded);
    for (int i = 0; i < 232; ++i) {
        assert(gd_get_frame(gif) == 1);
        assert(gif->gce.delay == 5 && gif->loop_count == 0);
        gd_render_frame(gif, gif->canvas);
        assert(std::fwrite(gif->canvas, 4, 400 * 180, decoded) == 400 * 180);
    }
    std::fclose(decoded);
    gd_close_gif(gif);

    lv_img_dsc_t descriptor = {};
    descriptor.data = bytes.data();
    descriptor.data_size = bytes.size();
    auto advance = [](int ms) {
        for (int i = 0; i < ms; i += 10) {
            lv_tick_inc(10);
            lv_timer_handler();
        }
    };
    lv_mem_monitor_t before = {}, after = {};
    lv_mem_monitor(&before);
    for (int pass = 0; pass < 12; ++pass) {
        LvglGif animation(&descriptor);
        assert(animation.IsLoaded() && animation.width() == 400 && animation.height() == 180);
        int callbacks = 0;
        animation.SetFrameCallback([&callbacks]() { ++callbacks; });
        animation.Start();
        advance(200);
        assert(animation.IsPlaying() && callbacks >= 3);
        animation.Pause();
        int paused = callbacks;
        advance(1000);
        assert(!animation.IsPlaying() && callbacks == paused);
        animation.Resume();
        advance(100);
        assert(animation.IsPlaying() && callbacks > paused);
        animation.Stop();
        assert(!animation.IsPlaying());
    }
    advance(20);
    lv_mem_monitor(&after);
    assert(after.free_size + 256 >= before.free_size);
    puts(
        "PASS: actual XiaoZhi decoder: 232 frames / two loops; actual LvglGif pause, resume, stop "
        "and repeated cleanup");
}
