#pragma once

#include <cstddef>
#include <cstdint>

namespace adrenalin::telemetry1::abi_v1 {

// Proposed ABI fixture for contract tests. This is not a production producer ABI.
// The only supported byte order is little-endian. All offsets/sizes are asserted.
inline constexpr std::uint16_t kMajor = 1;
inline constexpr std::uint16_t kMinor = 0;
inline constexpr std::uint32_t kSlotCount = 2;
inline constexpr std::uint32_t kMetricCount = 1;
inline constexpr char kMagic[8] = {'A','D','R','T','E','L','1','\0'};

enum class SampleState : std::uint32_t { Valid = 1, Unavailable = 2, Stale = 3 };
enum class MetricEncoding : std::uint32_t { UnsignedMicroUnits = 1 };

struct alignas(8) Header final {
    char magic[8];
    std::uint16_t abi_major;
    std::uint16_t abi_minor;
    std::uint32_t header_size;
    std::uint32_t mapped_size;
    std::uint32_t slot_count;
    std::uint32_t slot_size;
    std::uint32_t metric_count;
    std::uint32_t reserved0;
    std::uint64_t service_generation;
    std::uint64_t producer_generation;
    std::uint64_t metric_definition_generation;
    std::uint64_t subject_definition_generation;
    std::uint64_t reserved1;
};

// During publication every field uses the GCC/Clang lock-free atomic builtin
// contract. sequence_guard is the begin/end consistency counter;
// sample_sequence is a separate monotonic observation id. The wire ABI requires
// seq_cst operations, direct lock-free codegen, and address-free instructions.
struct alignas(8) Slot final {
    std::uint64_t sequence_guard;
    std::uint64_t sample_sequence;
    std::uint64_t monotonic_time_ns;
    std::uint32_t state;
    std::uint32_t encoding;
    std::uint64_t metric_value;
};

inline constexpr std::size_t kHeaderSize = sizeof(Header);
inline constexpr std::size_t kSlotSize = sizeof(Slot);
inline constexpr std::size_t kMappedSize = kHeaderSize + kSlotCount * kSlotSize;
static_assert(sizeof(Header) == 80);
static_assert(alignof(Header) == 8);
static_assert(sizeof(Slot) == 40);
static_assert(alignof(Slot) == 8);
static_assert(offsetof(Slot, sequence_guard) == 0);
static_assert(offsetof(Slot, sample_sequence) == 8);
static_assert(offsetof(Slot, monotonic_time_ns) == 16);
static_assert(offsetof(Slot, state) == 24);
static_assert(offsetof(Slot, metric_value) == 32);
static_assert(kMappedSize == 160);

} // namespace adrenalin::telemetry1::abi_v1
