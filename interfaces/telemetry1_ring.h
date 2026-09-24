#pragma once

#include "telemetry1_abi_v1.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace adrenalin::telemetry1::abi_v1 {

struct NegotiatedStream final {
    std::uint64_t service_generation;
    std::uint64_t producer_generation;
    std::uint64_t metric_definition_generation;
    std::uint64_t subject_definition_generation;
    std::uint16_t abi_major;
    std::uint64_t mapped_size;
};

struct ReadSample final {
    std::uint64_t sample_sequence = 0;
    std::uint64_t monotonic_time_ns = 0;
    SampleState state = SampleState::Unavailable;
    std::optional<std::uint64_t> value;
};

// ABI v1 is a Linux GCC/Clang implementation contract. Lock-free compiler
// builtins must lower directly to address-free hardware atomics, never process
// local locks. Unsupported compilers/targets fail at build time.
#if !defined(__GNUC__) && !defined(__clang__)
#error "Telemetry ABI v1 requires GCC/Clang lock-free process-shared atomics"
#endif
#if !defined(__x86_64__)
#error "This telemetry ABI fixture is limited to little-endian x86_64 Linux"
#endif
static_assert(sizeof(void *) == 8);
static_assert(__atomic_always_lock_free(sizeof(std::uint64_t), nullptr));
static_assert(__atomic_always_lock_free(sizeof(std::uint32_t), nullptr));
static_assert(alignof(Slot) >= alignof(std::uint64_t));

class SingleProducer final {
public:
    explicit SingleProducer(void *mapping, std::size_t size);
    bool isValid() const { return m_header != nullptr; }
    bool publish(SampleState state, std::uint64_t value, std::uint64_t monotonicTimeNs);
    std::uint64_t nextSampleSequence() const { return m_nextSequence; }

    // Deterministic corruption/exhaustion hooks used only by ABI tests.
    bool setSlotGuardForTest(std::uint32_t slot, std::uint64_t value);
    void setNextSequenceForTest(std::uint64_t value) { m_nextSequence = value; }
private:
    Header *m_header = nullptr;
    Slot *m_slots = nullptr;
    std::uint64_t m_nextSequence = 1;
};

bool readSlot(const void *mapping, std::size_t mappedSize,
              const NegotiatedStream &negotiated, std::uint32_t slotIndex,
              ReadSample *sample);

} // namespace adrenalin::telemetry1::abi_v1
