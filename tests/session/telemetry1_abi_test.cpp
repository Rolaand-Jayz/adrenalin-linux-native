#include "../../interfaces/telemetry1_abi_v1.h"
#include "../../interfaces/telemetry1_fixture_mock.h"
#include "../../interfaces/telemetry1_ring.h"
#include "../../interfaces/telemetry1_types.h"

#include <QDBusMetaType>
#include <QUuid>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <thread>
#include <type_traits>
#include <vector>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace adrenalin::telemetry1::abi_v1;
using namespace adrenalin::contracts::telemetry1;

namespace {
[[noreturn]] void checkFailed(const char *expression, const char *file, int line) {
    std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", file, line, expression);
    std::abort();
}
#define CHECK(expression) ((expression) ? static_cast<void>(0) : checkFailed(#expression, __FILE__, __LINE__))

class Region final {
public:
    Region() {
        const QByteArray name = QByteArray("/") + QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1();
        m_fd = ::shm_open(name.constData(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
        CHECK(m_fd >= 0);
        CHECK(::ftruncate(m_fd, static_cast<off_t>(kMappedSize)) == 0);
        m_mapping = ::mmap(nullptr, kMappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        CHECK(m_mapping != MAP_FAILED);
        CHECK(::shm_unlink(name.constData()) == 0);
        auto *header = new (m_mapping) Header{};
        auto *slotArray = reinterpret_cast<Slot *>(static_cast<char *>(m_mapping) + kHeaderSize);
        for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) new (&slotArray[slot]) Slot{};
        std::memcpy(header->magic, kMagic, sizeof(kMagic));
        header->abi_major = kMajor; header->abi_minor = kMinor;
        header->header_size = static_cast<std::uint32_t>(kHeaderSize);
        header->mapped_size = static_cast<std::uint32_t>(kMappedSize);
        header->slot_count = kSlotCount; header->slot_size = static_cast<std::uint32_t>(kSlotSize);
        header->metric_count = kMetricCount;
        header->service_generation = 4; header->producer_generation = 9;
        header->metric_definition_generation = 12; header->subject_definition_generation = 13;
    }
    ~Region() {
        if (m_mapping != MAP_FAILED) ::munmap(m_mapping, kMappedSize);
        if (m_fd >= 0) ::close(m_fd);
    }
    void *mapping() const { return m_mapping; }
    NegotiatedStream negotiated() const { return {4, 9, 12, 13, kMajor, kMappedSize}; }
private:
    int m_fd = -1;
    void *m_mapping = MAP_FAILED;
};

void testTypesAndReadOnlyFixture() {
    static_assert(std::is_standard_layout_v<Header> && std::is_trivially_copyable_v<Header>);
    static_assert(std::is_standard_layout_v<Slot> && std::is_trivially_copyable_v<Slot>);
    static_assert(__atomic_always_lock_free(sizeof(std::uint64_t), nullptr));
    static_assert(__atomic_always_lock_free(sizeof(std::uint32_t), nullptr));
    CHECK(kMajor == 1 && kMappedSize == 160);
    registerMetaTypes();
    CHECK(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<OpenResult>()))
           == QByteArray("(ssstttttqt)"));
    CHECK(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<QDBusUnixFileDescriptor>>()))
           == QByteArray("ah"));
    CHECK(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<MetricDefinition>>()))
           == QByteArray("a(ssss)"));
    CHECK(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<SubjectDefinition>>()))
           == QByteArray("a(ssss)"));

    FixtureMock mock;
    CHECK(mock.isValid());
    CHECK((::fcntl(mock.readerDescriptor(), F_GETFL) & O_ACCMODE) == O_RDONLY);
    void *writeMap = ::mmap(nullptr, kMappedSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                            mock.readerDescriptor(), 0);
    CHECK(writeMap == MAP_FAILED);
    void *readMap = ::mmap(nullptr, kMappedSize, PROT_READ, MAP_SHARED,
                           mock.readerDescriptor(), 0);
    CHECK(readMap != MAP_FAILED);
    const auto result = mock.openResult();
    NegotiatedStream bound{result.serviceGeneration, result.producerGeneration,
        result.metricDefinitionGeneration, result.subjectDefinitionGeneration,
        result.abiMajor, result.mappedSize};
    ReadSample sample;
    CHECK(readSlot(readMap, kMappedSize, bound, 0, &sample));
    CHECK(sample.state == SampleState::Valid && sample.value == 60'000'000);
    CHECK(::munmap(readMap, kMappedSize) == 0);
}

void testPublicationStateWrapRejectionAndExhaustion() {
    Region region;
    SingleProducer producer(region.mapping(), kMappedSize);
    CHECK(producer.isValid());
    ReadSample sample;
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    CHECK(producer.publish(SampleState::Valid, 111, 1));
    CHECK(readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    CHECK(sample.sample_sequence == 1 && sample.value == 111);

    auto *slot0 = reinterpret_cast<Slot *>(static_cast<char *>(region.mapping()) + kHeaderSize);
    __atomic_store_n(&slot0->sequence_guard, std::uint64_t(3), __ATOMIC_SEQ_CST);
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    __atomic_store_n(&slot0->sequence_guard, std::uint64_t(2), __ATOMIC_SEQ_CST);
    CHECK(readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));

    CHECK(producer.publish(SampleState::Unavailable, 999, 2));
    CHECK(readSlot(region.mapping(), kMappedSize, region.negotiated(), 1, &sample));
    CHECK(sample.state == SampleState::Unavailable && !sample.value.has_value());
    CHECK(producer.publish(SampleState::Stale, 999, 3));
    CHECK(readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    CHECK(sample.sample_sequence == 3 && sample.state == SampleState::Stale);
    CHECK(!sample.value.has_value());
    CHECK(producer.publish(SampleState::Valid, 444, 4));
    CHECK(readSlot(region.mapping(), kMappedSize, region.negotiated(), 1, &sample));
    CHECK(sample.sample_sequence == 4 && sample.value == 444);
    CHECK(producer.publish(SampleState::Valid, 555, 5));
    CHECK(readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    CHECK(sample.sample_sequence == 5 && sample.value == 555);

    __atomic_store_n(&slot0->sample_sequence, std::uint64_t(3), __ATOMIC_SEQ_CST);
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    __atomic_store_n(&slot0->sample_sequence, std::uint64_t(5), __ATOMIC_SEQ_CST);
    __atomic_store_n(&slot0->sample_sequence, std::numeric_limits<std::uint64_t>::max(),
                     __ATOMIC_SEQ_CST);
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    __atomic_store_n(&slot0->sample_sequence, std::uint64_t(5), __ATOMIC_SEQ_CST);
    __atomic_store_n(&slot0->sequence_guard, std::uint64_t(4), __ATOMIC_SEQ_CST);
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    __atomic_store_n(&slot0->sequence_guard, std::uint64_t(6), __ATOMIC_SEQ_CST);

    auto *slot1 = reinterpret_cast<Slot *>(static_cast<char *>(region.mapping()) + kHeaderSize)
        + 1;
    __atomic_store_n(&slot1->sample_sequence, std::uint64_t(3), __ATOMIC_SEQ_CST);
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 1, &sample));
    __atomic_store_n(&slot1->sample_sequence, std::uint64_t(4), __ATOMIC_SEQ_CST);

    const auto baseline = region.negotiated();
    const auto rejectsNegotiation = [&](NegotiatedStream candidate) {
        CHECK(!readSlot(region.mapping(), kMappedSize, candidate, 0, &sample));
    };
    auto changed = baseline;
    changed.service_generation++;
    rejectsNegotiation(changed);
    changed = baseline;
    changed.producer_generation++;
    rejectsNegotiation(changed);
    changed = baseline;
    changed.metric_definition_generation++;
    rejectsNegotiation(changed);
    changed = baseline;
    changed.subject_definition_generation++;
    rejectsNegotiation(changed);
    changed = baseline;
    changed.abi_major++;
    rejectsNegotiation(changed);
    changed = baseline;
    changed.mapped_size++;
    rejectsNegotiation(changed);
    CHECK(!readSlot(region.mapping(), kMappedSize - 1, baseline, 0, &sample));

    auto *header = static_cast<Header *>(region.mapping());
    ++header->service_generation;
    CHECK(!readSlot(region.mapping(), kMappedSize, baseline, 0, &sample));
    --header->service_generation;
    ++header->producer_generation;
    CHECK(!readSlot(region.mapping(), kMappedSize, baseline, 0, &sample));
    --header->producer_generation;
    ++header->metric_definition_generation;
    CHECK(!readSlot(region.mapping(), kMappedSize, baseline, 0, &sample));
    --header->metric_definition_generation;
    ++header->subject_definition_generation;
    CHECK(!readSlot(region.mapping(), kMappedSize, baseline, 0, &sample));
    --header->subject_definition_generation;
    ++header->abi_major;
    CHECK(!readSlot(region.mapping(), kMappedSize, baseline, 0, &sample));
    --header->abi_major;
    ++header->mapped_size;
    CHECK(!readSlot(region.mapping(), kMappedSize, baseline, 0, &sample));
    --header->mapped_size;

    __atomic_store_n(&slot0->state, std::uint32_t(99), __ATOMIC_SEQ_CST);
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));

    Region exhaustedRegion;
    SingleProducer exhausted(exhaustedRegion.mapping(), kMappedSize);
    CHECK(exhausted.isValid());
    CHECK(exhausted.setSlotGuardForTest(0, std::numeric_limits<std::uint64_t>::max() - 1U));
    CHECK(!exhausted.publish(SampleState::Valid, 1, 1));
    CHECK(exhausted.nextSampleSequence() == 1);
    const auto *exhaustedSlot = reinterpret_cast<const Slot *>(
        static_cast<const char *>(exhaustedRegion.mapping()) + kHeaderSize);
    CHECK(__atomic_load_n(&exhaustedSlot->sequence_guard, __ATOMIC_SEQ_CST) == std::numeric_limits<std::uint64_t>::max() - 1U);
    exhausted.setNextSequenceForTest(std::numeric_limits<std::uint64_t>::max());
    CHECK(!exhausted.publish(SampleState::Valid, 2, 2));
    CHECK(exhausted.nextSampleSequence() == std::numeric_limits<std::uint64_t>::max());
}

void testMalformedMappingsFailClosed() {
    Region region;
    auto *header = static_cast<Header *>(region.mapping());
    const Header validHeader = *header;
    ReadSample sample;

    CHECK(!readSlot(nullptr, kMappedSize, region.negotiated(), 0, &sample));
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, nullptr));
    CHECK(!readSlot(static_cast<const char *>(region.mapping()) + 1, kMappedSize,
                    region.negotiated(), 0, &sample));
    CHECK(!readSlot(region.mapping(), kMappedSize - 1, region.negotiated(), 0, &sample));
    CHECK(!readSlot(region.mapping(), kMappedSize + 1, region.negotiated(), 0, &sample));
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), kSlotCount, &sample));

    const std::array<void (*)(Header &), 12> corruptHeaders{
        [](Header &value) { value.magic[0] ^= 0xffU; },
        [](Header &value) { ++value.abi_major; },
        [](Header &value) { ++value.header_size; },
        [](Header &value) { ++value.mapped_size; },
        [](Header &value) { ++value.slot_count; },
        [](Header &value) { ++value.slot_size; },
        [](Header &value) { ++value.metric_count; },
        [](Header &value) { ++value.service_generation; },
        [](Header &value) { ++value.producer_generation; },
        [](Header &value) { ++value.metric_definition_generation; },
        [](Header &value) { ++value.subject_definition_generation; },
        [](Header &value) { value.service_generation = 0; }
    };
    for (const auto corruptHeader : corruptHeaders) {
        *header = validHeader;
        corruptHeader(*header);
        CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    }
    *header = validHeader;

    SingleProducer producer(region.mapping(), kMappedSize);
    CHECK(producer.isValid());
    auto *slot = reinterpret_cast<Slot *>(static_cast<char *>(region.mapping()) + kHeaderSize);
    CHECK(producer.publish(SampleState::Valid, 42, 10));
    const Slot validSlot = *slot;

    slot->sample_sequence = 0;
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    *slot = validSlot;
    slot->encoding = static_cast<std::uint32_t>(MetricEncoding::UnsignedMicroUnits) + 1U;
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    *slot = validSlot;
    slot->sequence_guard = 0;
    CHECK(!readSlot(region.mapping(), kMappedSize, region.negotiated(), 0, &sample));
    *slot = validSlot;

    const auto initial = region.negotiated();
    auto zeroGeneration = initial;
    zeroGeneration.service_generation = 0;
    CHECK(!readSlot(region.mapping(), kMappedSize, zeroGeneration, 0, &sample));
    auto zeroProducerGeneration = initial;
    zeroProducerGeneration.producer_generation = 0;
    CHECK(!readSlot(region.mapping(), kMappedSize, zeroProducerGeneration, 0, &sample));
}

void testForkedProducerConcurrentReaders() {
    Region region;
    const pid_t child = ::fork();
    CHECK(child >= 0);
    if (child == 0) {
        SingleProducer producer(region.mapping(), kMappedSize);
        if (!producer.isValid()) ::_exit(2);
        for (std::uint64_t sequence = 1; sequence <= 100'000; ++sequence) {
            if (!producer.publish(SampleState::Valid, sequence * 17U, sequence)) ::_exit(3);
        }
        ::_exit(0);
    }

    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> accepted{0};
    std::atomic<std::uint64_t> rejected{0};
    std::atomic<std::uint64_t> corrupt{0};
    std::vector<std::thread> readers;
    for (std::uint32_t reader = 0; reader < 4; ++reader) {
        readers.emplace_back([&, reader] {
            while (!stop.load(std::memory_order_relaxed)) {
                for (std::uint32_t slotIndex = 0; slotIndex < kSlotCount; ++slotIndex) {
                    ReadSample sample;
                    if (!readSlot(region.mapping(), kMappedSize, region.negotiated(),
                                  (slotIndex + reader) % kSlotCount, &sample)) {
                        rejected.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }
                    accepted.fetch_add(1, std::memory_order_relaxed);
                    if (!sample.value || sample.state != SampleState::Valid
                        || sample.sample_sequence > 100'000
                        || *sample.value != sample.sample_sequence * 17U
                        || sample.monotonic_time_ns != sample.sample_sequence)
                        corrupt.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    int status = 0;
    CHECK(::waitpid(child, &status, 0) == child);
    stop.store(true, std::memory_order_relaxed);
    for (auto &reader : readers) reader.join();
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(accepted.load() > 0);
    CHECK(corrupt.load() == 0);
}

void testProducerRejectsMalformedHeaders() {
    const auto rejectsProducer = [](void (*corrupt)(Header &)) {
        Region region;
        corrupt(*static_cast<Header *>(region.mapping()));
        SingleProducer producer(region.mapping(), kMappedSize);
        CHECK(!producer.isValid());
        CHECK(!producer.publish(SampleState::Valid, 1, 1));
    };
    rejectsProducer([](Header &header) { header.magic[0] ^= 0xffU; });
    rejectsProducer([](Header &header) { ++header.metric_count; });
}
}

int main() {
    testTypesAndReadOnlyFixture();
    testPublicationStateWrapRejectionAndExhaustion();
    testMalformedMappingsFailClosed();
    testProducerRejectsMalformedHeaders();
    testForkedProducerConcurrentReaders();
}
