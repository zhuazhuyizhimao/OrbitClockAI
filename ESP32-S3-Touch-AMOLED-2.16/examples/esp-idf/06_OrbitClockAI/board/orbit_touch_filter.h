#pragma once
#include <cstdint>

namespace orbit_touch {
enum class Report { Contact, Released, Missing };
struct Sample {
    bool down = false;
    int x = 0, y = 0;
};

// Missing/invalid reports are not physical releases. Debounce explicit releases
// too, but bound retention so a disconnected controller cannot latch a finger.
class ContactFilter {
public:
    // CST92xx can signal a stationary touch at one-second intervals. A valid
    // release still completes after 80ms; missing reports get a bounded grace.
    static constexpr uint32_t RELEASE_MS = 80, STALE_MS = 1200;
    Sample Update(Report report, int x, int y, uint32_t now) {
        if (report == Report::Contact) {
            sample_ = {true, x, y};
            last_contact_ = now;
            release_pending_ = false;
        } else if (sample_.down) {
            if (report == Report::Released && !release_pending_) {
                release_pending_ = true;
                release_at_ = now;
            }
            if ((release_pending_ && uint32_t(now - release_at_) >= RELEASE_MS) ||
                uint32_t(now - last_contact_) >= STALE_MS) {
                sample_.down = false;
                release_pending_ = false;
            }
        }
        return sample_;
    }
    void Reset() { *this = {}; }

private:
    Sample sample_;
    uint32_t last_contact_ = 0, release_at_ = 0;
    bool release_pending_ = false;
};
}  // namespace orbit_touch
