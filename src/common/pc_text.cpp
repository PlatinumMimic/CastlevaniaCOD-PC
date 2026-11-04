// The game's own messages that named the Xbox, for a PC: "Saving... Please don't turn off your Xbox console." and the
// rest of the save, load and delete messages say "your PC" instead. They are the US release's English messages
// (data\global2ue.dar, plain text); each is changed in place as the file is read (the SDK's kernel::g_file_read), the
// new words no longer than the old with the rest of their space zeroed, and only where the bytes read are the ones
// this was made from: another release's file is left as it is.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

#include "core/log.h"
#include "kernel/kernel.h"

namespace {

struct Message {
    uint32_t at;      // where it starts in the file
    const char* was;  // the release's words
    const char* now;  // the PC's, no longer
};

// every message of data\global2ue.dar with "Xbox" in it
constexpr Message kMessages[] = {
    {0x987d8, "Saving...\nPlease don't turn off your Xbox console.",
     "Saving...\nPlease don't turn off your PC."},
    {0x98918, "No game data present on your Xbox.",
     "No game data present on your PC."},
    {0x98aa8, "Failed to load data from your Xbox.",
     "Failed to load data from your PC."},
    {0x98b70, "Loading data from your Xbox.\nPlease don't turn off your Xbox console.",
     "Loading data from your PC.\nPlease don't turn off your PC."},
    {0x98bc0, "Loading...\nPlease don't turn off your Xbox console.",
     "Loading...\nPlease don't turn off your PC."},
    {0x98ca0, "Data loaded from your Xbox.",
     "Data loaded from your PC."},
    {0x98eb8, "Deleting...\nPlease don't turn off your Xbox console.",
     "Deleting...\nPlease don't turn off your PC."},
    {0x98ef8, "Creating new save data...\nPlease don't turn off your Xbox console.",
     "Creating new save data...\nPlease don't turn off your PC."},
    {0x993e0, "Your Xbox doesn't have enough \nfree blocks to save games.\nYou need to free %d more blocks.\nPress A to continue without saving\nor B to free more blocks.",
     "Your PC doesn't have enough \nfree blocks to save games.\nYou need to free %d more blocks.\nPress A to continue without saving\nor B to free more blocks."},
    {0x99b00, "There is no save data on your Xbox.\nCreate new save data?",
     "There is no save data on your PC.\nCreate new save data?"},
    {0x99c30, "Checking...\nPlease don't turn off your Xbox console.",
     "Checking...\nPlease don't turn off your PC."},
};
static_assert([] {
    for (const Message& m : kMessages)
        if (std::char_traits<char>::length(m.now) > std::char_traits<char>::length(m.was)) return false;
    return true;
}(), "a PC message is longer than the one it replaces");

bool EndsWith(const std::string& s, const char* tail) {
    size_t n = std::strlen(tail);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (std::tolower(static_cast<unsigned char>(s[s.size() - n + i])) != tail[i]) return false;
    return true;
}

// the part of each message this read holds (a message can straddle two reads): changed when it is the release's
void Read(const std::string& path, uint64_t offset, uint8_t* data, size_t size) {
    if (!EndsWith(path, "global2ue.dar")) return;
    int changed = 0;
    for (const Message& m : kMessages) {
        size_t len = std::strlen(m.was) + 1, now = std::strlen(m.now);
        uint64_t lo = std::max<uint64_t>(offset, m.at), hi = std::min<uint64_t>(offset + size, m.at + len);
        if (lo >= hi) continue;
        size_t from = size_t(lo - m.at), n = size_t(hi - lo);
        uint8_t* p = data + (lo - offset);
        // the release's bytes there (its zero at the end too)
        if (std::memcmp(p, m.was + from, n) != 0) continue;
        for (size_t i = 0; i < n; i++) p[i] = from + i < now ? uint8_t(m.now[from + i]) : 0;
        changed++;
    }
    if (changed) LOGR("pc text: %d of the save messages say \"your PC\" (%s)", changed, path.c_str());
}

}  // namespace

void PcTextInit() { kernel::g_file_read = Read; }
