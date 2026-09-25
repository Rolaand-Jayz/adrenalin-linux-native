#include "telemetry1_ring.h"

#include <cstring>
#include <bit>
#include <limits>

namespace adrenalin::telemetry1::abi_v1 {
namespace {
std::uint64_t load(std::uint64_t &field) {
    return __atomic_load_n(&field, __ATOMIC_SEQ_CST);
}
std::uint32_t load(std::uint32_t &field) {
    return __atomic_load_n(&field, __ATOMIC_SEQ_CST);
}
void store(std::uint64_t &field, std::uint64_t value) {
    __atomic_store_n(&field, value, __ATOMIC_SEQ_CST);
}
void store(std::uint32_t &field, std::uint32_t value) {
    __atomic_store_n(&field, value, __ATOMIC_SEQ_CST);
}
bool validState(std::uint32_t state) {
    return state == static_cast<std::uint32_t>(SampleState::Valid)
        || state == static_cast<std::uint32_t>(SampleState::Unavailable)
        || state == static_cast<std::uint32_t>(SampleState::Stale);
}
bool validHeader(const Header &header, std::size_t size, const NegotiatedStream &n) {
    return size >= sizeof(Header)
        && std::memcmp(header.magic, kMagic, sizeof(kMagic)) == 0
        && header.abi_major == kMajor && header.abi_minor == kMinor
        && n.abi_major == kMajor
        && header.header_size == sizeof(Header)
        && header.mapped_size == size && n.mapped_size == size
        && header.slot_count == kSlotCount && header.slot_size == sizeof(Slot)
        && header.metric_count == kMetricCount
        && header.reserved0 == 0 && header.reserved1 == 0
        && header.service_generation == n.service_generation
        && header.producer_generation == n.producer_generation
        && header.metric_definition_generation == n.metric_definition_generation
        && header.subject_definition_generation == n.subject_definition_generation;
}
}

SingleProducer::SingleProducer(void *mapping, std::size_t size) {
    if constexpr (std::endian::native != std::endian::little) return;
    if (!mapping || size != kMappedSize || reinterpret_cast<std::uintptr_t>(mapping) % alignof(Header))
        return;
    auto *header = static_cast<Header *>(mapping);
    if (std::memcmp(header->magic, kMagic, sizeof(kMagic)) != 0
        || header->abi_major != kMajor || header->abi_minor != kMinor
        || header->header_size != sizeof(Header)
        || header->mapped_size != size || header->slot_count != kSlotCount
        || header->slot_size != sizeof(Slot) || header->metric_count != kMetricCount
        || header->reserved0 != 0 || header->reserved1 != 0
        || header->producer_generation == 0
        || header->metric_definition_generation == 0
        || header->subject_definition_generation == 0 || header->service_generation == 0)
        return;
    m_header = header;
    m_slots = reinterpret_cast<Slot *>(static_cast<char *>(mapping) + sizeof(Header));
    for (std::uint32_t i = 0; i < kSlotCount; ++i) {
        if (load(m_slots[i].sequence_guard) != 0) {
            m_header = nullptr; m_slots = nullptr; return;
        }
    }
}

bool SingleProducer::publish(SampleState state, std::uint64_t value,
                             std::uint64_t monotonicTimeNs) {
    if (!m_header || m_nextSequence == 0
        || m_nextSequence == std::numeric_limits<std::uint64_t>::max()) return false;
    const std::uint64_t sequence = m_nextSequence;
    const std::uint32_t index = static_cast<std::uint32_t>((sequence - 1) % kSlotCount);
    Slot &slot = m_slots[index];
    const std::uint64_t priorGuard = load(slot.sequence_guard);
    if ((priorGuard & 1U) != 0U
        || priorGuard > std::numeric_limits<std::uint64_t>::max() - 3U) return false;
    if (state != SampleState::Valid && state != SampleState::Unavailable
        && state != SampleState::Stale) return false;
    const std::uint64_t oddGuard = priorGuard + 1U;
    const std::uint64_t committedGuard = priorGuard + 2U;
    store(slot.sequence_guard, oddGuard);
    store(slot.sample_sequence, sequence);
    store(slot.monotonic_time_ns, monotonicTimeNs);
    store(slot.state, static_cast<std::uint32_t>(state));
    store(slot.encoding, static_cast<std::uint32_t>(MetricEncoding::UnsignedMicroUnits));
    store(slot.metric_value, state == SampleState::Valid ? value : 0U);
    store(slot.sequence_guard, committedGuard);
    ++m_nextSequence;
    return true;
}

bool SingleProducer::setSlotGuardForTest(std::uint32_t slot, std::uint64_t value) {
    if (!m_header || slot >= kSlotCount) return false;
    store(m_slots[slot].sequence_guard, value);
    return true;
}

bool readSlot(const void *mapping, std::size_t mappedSize,
              const NegotiatedStream &negotiated, std::uint32_t slotIndex,
              ReadSample *sample) {
    if (!mapping || !sample || slotIndex >= kSlotCount
        || mappedSize != kMappedSize
        || reinterpret_cast<std::uintptr_t>(mapping) % alignof(Header)) return false;
    if constexpr (std::endian::native != std::endian::little) return false;
    const auto *header = static_cast<const Header *>(mapping);
    if (!validHeader(*header, mappedSize, negotiated)) return false;
    auto *slots = reinterpret_cast<Slot *>(const_cast<char *>(static_cast<const char *>(mapping))
                                           + sizeof(Header));
    Slot &slot = slots[slotIndex];
    const auto first = load(slot.sequence_guard);
    if (first == 0 || (first & 1U) != 0U) return false;
    ReadSample candidate;
    candidate.sample_sequence = load(slot.sample_sequence);
    candidate.monotonic_time_ns = load(slot.monotonic_time_ns);
    const auto state = load(slot.state);
    const auto encoding = load(slot.encoding);
    const auto value = load(slot.metric_value);
    const auto second = load(slot.sequence_guard);
    if (first != second || (second & 1U) != 0U || candidate.sample_sequence == 0
        || candidate.sample_sequence == std::numeric_limits<std::uint64_t>::max()
        || (candidate.sample_sequence - 1U) % kSlotCount != slotIndex
        || second != candidate.sample_sequence + (candidate.sample_sequence & 1U)
        || !validState(state)
        || encoding != static_cast<std::uint32_t>(MetricEncoding::UnsignedMicroUnits)) return false;
    candidate.state = static_cast<SampleState>(state);
    if (candidate.state == SampleState::Valid) candidate.value = value;
    *sample = candidate;
    return true;
}

} // namespace adrenalin::telemetry1::abi_v1
