// SPDX-License-Identifier: GPL-3.0-only
// Offline sound bank renderer for DDSOM N64: runs the game's Z80 sound program (v1.15)
// on the reference QSound HLE chip to produce mono 12019 Hz WAV files and snd/table.bin.
// This tool runs only on the host during asset preparation and is never linked into the N64 ROM.

#include "sound/qsound.h"
#include "sound/z80.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace cps2recomp;
namespace fs = std::filesystem;

namespace {

constexpr int kCodes = 0x490;
constexpr int kOutputRate = QSound::kSampleRate / 2; // 12019 Hz
constexpr uint64_t kSoundClock = 8000000ULL;
constexpr uint64_t kInterruptPeriod = 32000ULL; // 250 Hz (4 ms)
constexpr int kMaxSeconds = 60;
constexpr int kSilenceThreshold = 24;
constexpr int kSilenceSeconds = 2;
constexpr size_t kTailPadSamples = 240; // ~20 ms tail padding at 12019 Hz

struct OfflineBus final : Z80Bus {
    const std::vector<uint8_t> &rom;
    const std::vector<uint8_t> &ops;
    QSound &qsound;
    uint8_t shared[0x1000]{};
    uint8_t ram[0x1000]{};
    uint8_t bank = 0;
    bool ready = false;

    OfflineBus(const std::vector<uint8_t> &r, const std::vector<uint8_t> &o, QSound &q)
        : rom(r), ops(o), qsound(q) {
        std::memset(shared, 0, sizeof(shared));
        std::memset(ram, 0, sizeof(ram));
        shared[0x00F] = 0xFF;
        shared[0xFFD] = 0x88;
        shared[0xFFE] = 0x01;
    }

    uint8_t Read(uint16_t a) override {
        if (a < 0x8000) {
            return a < rom.size() ? rom[a] : 0xFF;
        }
        if (a < 0xC000) {
            const size_t off = 0x8000 + static_cast<size_t>(bank) * 0x4000 + (a - 0x8000);
            return off < rom.size() ? rom[off] : 0xFF;
        }
        if (a < 0xD000) {
            if (a == 0xCFFF && shared[0xFFF] == 0x77) {
                shared[0xFFF] = 0xFF;
                ready = true;
            }
            return shared[a - 0xC000];
        }
        if (a == 0xD007) {
            return qsound.Read();
        }
        if (a >= 0xF000) {
            return ram[a - 0xF000];
        }
        return 0xFF;
    }

    uint8_t ReadOpcode(uint16_t a) override {
        return a < ops.size() ? ops[a] : Read(a);
    }

    void Write(uint16_t a, uint8_t v) override {
        if (a >= 0xC000 && a < 0xD000) {
            shared[a - 0xC000] = v;
        } else if (a >= 0xD000 && a <= 0xD002) {
            qsound.Write(a - 0xD000, v);
        } else if (a == 0xD003) {
            bank = v & 0x0F;
        } else if (a >= 0xF000) {
            ram[a - 0xF000] = v;
        }
    }
};

std::vector<uint8_t> LoadFile(const fs::path &path) {
    FILE *f = std::fopen(path.string().c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "gen_sound_bank: cannot open %s\n", path.string().c_str());
        std::exit(1);
    }
    std::fseek(f, 0, SEEK_END);
    const long len = std::ftell(f);
    std::rewind(f);
    std::vector<uint8_t> out(len > 0 ? static_cast<size_t>(len) : 0);
    if (!out.empty() && std::fread(out.data(), 1, out.size(), f) != out.size()) {
        std::fprintf(stderr, "gen_sound_bank: truncated read %s\n", path.string().c_str());
        std::exit(1);
    }
    std::fclose(f);
    return out;
}

void WriteWav16Mono(const fs::path &path, const std::vector<int16_t> &samples, uint32_t sampleRate) {
    FILE *f = std::fopen(path.string().c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "gen_sound_bank: cannot write %s\n", path.string().c_str());
        std::exit(1);
    }
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t riffSize = 36 + dataBytes;
    const uint32_t fmtSize = 16;
    const uint16_t audioFmt = 1;
    const uint16_t channels = 1;
    const uint32_t byteRate = sampleRate * 2;
    const uint16_t blockAlign = 2;
    const uint16_t bitsPerSample = 16;

    std::fwrite("RIFF", 1, 4, f);
    std::fwrite(&riffSize, 4, 1, f);
    std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmtSize, 4, 1, f);
    std::fwrite(&audioFmt, 2, 1, f);
    std::fwrite(&channels, 2, 1, f);
    std::fwrite(&sampleRate, 4, 1, f);
    std::fwrite(&byteRate, 4, 1, f);
    std::fwrite(&blockAlign, 2, 1, f);
    std::fwrite(&bitsPerSample, 2, 1, f);
    std::fwrite("data", 1, 4, f);
    std::fwrite(&dataBytes, 4, 1, f);
    if (!samples.empty()) {
        std::fwrite(samples.data(), sizeof(int16_t), samples.size(), f);
    }
    std::fclose(f);
}

uint64_t HashPcm(const std::vector<int16_t> &pcm) {
    uint64_t h = 14695981039346656037ULL;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(pcm.data());
    const size_t n = pcm.size() * sizeof(int16_t);
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

struct RenderedSound {
    bool active = false;
    bool music = false;
    uint64_t hash = 0;
    std::vector<int16_t> pcm;
};

RenderedSound RenderCode(int code, const std::vector<uint8_t> &rom, const std::vector<uint8_t> &ops,
                         const std::vector<uint8_t> &samples) {
    QSound qsound(samples.data(), samples.size());
    OfflineBus bus(rom, ops, qsound);
    Z80 cpu(bus);
    cpu.Reset();
    cpu.cycles = 0;

    uint64_t irqAt = kInterruptPeriod;
    uint64_t sampleCount = 0;
    uint64_t sampleAt = kSoundClock / QSound::kSampleRate;
    int bootTicks = 0;
    bool sent = false;

    std::vector<int16_t> mono24;
    mono24.reserve(static_cast<size_t>(kMaxSeconds) * QSound::kSampleRate + 64);

    bool started = false;
    size_t firstActive = 0;
    size_t lastActive = 0;
    size_t silentRun = 0;

    while (true) {
        const uint64_t next = std::min(irqAt, sampleAt);
        cpu.Run(next, Z80::Mode::Interpreter);
        cpu.cycles = std::max(cpu.cycles, next);
        while (irqAt <= cpu.cycles) {
            irqAt += kInterruptPeriod;
            cpu.RaiseInterrupt();
            bootTicks++;
        }
        if (!sent && bus.ready && bootTicks >= 25 && bus.shared[15] == 0xFF) {
            std::memset(bus.shared, 0, 16);
            bus.shared[0] = static_cast<uint8_t>(code >> 8);
            bus.shared[1] = static_cast<uint8_t>(code & 0xFF);
            bus.shared[7] = 16; // center pan
            bus.shared[15] = 0;
            sent = true;
        }
        while (sampleAt <= cpu.cycles) {
            int16_t lr[2];
            qsound.Step(lr);
            if (sent) {
                const int16_t m = static_cast<int16_t>((static_cast<int32_t>(lr[0]) + static_cast<int32_t>(lr[1])) / 2);
                mono24.push_back(m);
                if (std::abs(m) > kSilenceThreshold) {
                    if (!started) {
                        started = true;
                        firstActive = mono24.size() - 1;
                    }
                    lastActive = mono24.size() - 1;
                    silentRun = 0;
                } else {
                    silentRun++;
                    if (started && silentRun > static_cast<size_t>(kSilenceSeconds) * QSound::kSampleRate) {
                        goto finished;
                    }
                    if (!started && mono24.size() > static_cast<size_t>(kSilenceSeconds) * QSound::kSampleRate) {
                        goto finished;
                    }
                }
                if (mono24.size() >= static_cast<size_t>(kMaxSeconds) * QSound::kSampleRate) {
                    goto finished;
                }
            }
            sampleCount++;
            sampleAt = (sampleCount + 1) * kSoundClock / QSound::kSampleRate;
        }
    }

finished:
    RenderedSound out;
    if (!started) {
        return out;
    }

    const bool isMusic = (mono24.size() >= static_cast<size_t>(kMaxSeconds) * QSound::kSampleRate &&
                          silentRun < static_cast<size_t>(QSound::kSampleRate));
    size_t end24 = isMusic ? (mono24.size() - 1) : std::min(mono24.size() - 1, lastActive + kTailPadSamples * 2);

    out.active = true;
    out.music = isMusic;
    out.pcm.reserve((end24 - firstActive + 1) / 2);
    for (size_t i = firstActive; i + 1 <= end24; i += 2) {
        const int32_t avg = (static_cast<int32_t>(mono24[i]) + static_cast<int32_t>(mono24[i + 1])) / 2;
        out.pcm.push_back(static_cast<int16_t>(avg));
    }
    if (out.pcm.empty()) {
        out.active = false;
        return out;
    }
    out.hash = HashPcm(out.pcm);
    return out;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 6) {
        std::fprintf(stderr, "Usage: gen_sound_bank <z80.bin> <z80_ops.bin> <samples.bin> <wav_out_dir> <table.bin>\n");
        return 1;
    }

    const auto rom = LoadFile(argv[1]);
    const auto ops = LoadFile(argv[2]);
    const auto samples = LoadFile(argv[3]);
    const fs::path wavDir = argv[4];
    const fs::path tablePath = argv[5];

    fs::create_directories(wavDir / "sfx");
    fs::create_directories(wavDir / "music");
    fs::create_directories(tablePath.parent_path());

    std::vector<RenderedSound> rendered(kCodes);
    std::atomic<int> nextCode{0};
    const unsigned nThreads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    workers.reserve(nThreads);
    for (unsigned t = 0; t < nThreads; t++) {
        workers.emplace_back([&]() {
            while (true) {
                const int code = nextCode.fetch_add(1);
                if (code >= kCodes) {
                    break;
                }
                rendered[code] = RenderCode(code, rom, ops, samples);
            }
        });
    }
    for (auto &w : workers) {
        w.join();
    }

    std::unordered_map<uint64_t, std::vector<uint16_t>> byHash;
    std::vector<uint8_t> table(kCodes * 3, 0);
    int uniqueSfx = 0, uniqueMusic = 0, emptyCount = 0;

    for (int c = 0; c < kCodes; c++) {
        uint16_t fid = 0xFFFF;
        uint8_t music = 0;
        if (rendered[c].active) {
            auto &bucket = byHash[rendered[c].hash];
            for (uint16_t prev : bucket) {
                if (rendered[prev].music == rendered[c].music && rendered[prev].pcm == rendered[c].pcm) {
                    fid = prev;
                    break;
                }
            }
            if (fid == 0xFFFF) {
                fid = static_cast<uint16_t>(c);
                bucket.push_back(fid);
                char name[32];
                std::snprintf(name, sizeof(name), "s%04X.wav", fid);
                const fs::path outPath = wavDir / (rendered[c].music ? "music" : "sfx") / name;
                WriteWav16Mono(outPath, rendered[c].pcm, kOutputRate);
                if (rendered[c].music) {
                    uniqueMusic++;
                } else {
                    uniqueSfx++;
                }
            }
            music = rendered[c].music ? 1 : 0;
        } else {
            emptyCount++;
        }
        table[c * 3 + 0] = static_cast<uint8_t>(fid >> 8);
        table[c * 3 + 1] = static_cast<uint8_t>(fid & 0xFF);
        table[c * 3 + 2] = music;
    }

    FILE *tf = std::fopen(tablePath.string().c_str(), "wb");
    if (!tf) {
        std::fprintf(stderr, "gen_sound_bank: cannot write %s\n", tablePath.string().c_str());
        return 1;
    }
    std::fwrite(table.data(), 1, table.size(), tf);
    std::fclose(tf);

    std::printf("gen_sound_bank: %d codes -> %d unique WAVs (%d sfx, %d music), %d empty; wrote %s\n",
                kCodes, uniqueSfx + uniqueMusic, uniqueSfx, uniqueMusic, emptyCount, tablePath.string().c_str());
    return 0;
}

