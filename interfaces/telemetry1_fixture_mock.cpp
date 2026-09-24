#include "telemetry1_fixture_mock.h"
#include "telemetry1_ring.h"

#include <QFile>
#include <QUuid>
#include <bit>
#include <QDBusUnixFileDescriptor>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <new>

namespace adrenalin::contracts::telemetry1 {
using namespace adrenalin::telemetry1::abi_v1;
FixtureMock::FixtureMock() {
    if constexpr (std::endian::native != std::endian::little) return;
    m_instance = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // POSIX shm_open requires a leading slash; this is an ephemeral shm identifier,
    // not a filesystem path. UUID avoids a fixed name/prefix and O_EXCL prevents reuse.
    const QByteArray name = QFile::encodeName(QStringLiteral("/") + m_instance);
    m_producerFd = ::shm_open(name.constData(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (m_producerFd < 0) return;
    if (::ftruncate(m_producerFd, static_cast<off_t>(kMappedSize)) != 0) {
        ::shm_unlink(name.constData()); ::close(m_producerFd); m_producerFd = -1; return;
    }
    void *mapping = ::mmap(nullptr, kMappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_producerFd, 0);
    if (mapping == MAP_FAILED) {
        ::shm_unlink(name.constData()); ::close(m_producerFd); m_producerFd = -1; return;
    }
    auto *header = new (mapping) Header{};
    auto *slotArray = reinterpret_cast<Slot *>(static_cast<char *>(mapping) + kHeaderSize);
    for (std::uint32_t slot = 0; slot < kSlotCount; ++slot) new (&slotArray[slot]) Slot{};
    std::memcpy(header->magic, kMagic, sizeof(kMagic));
    header->abi_major = kMajor; header->abi_minor = kMinor;
    header->header_size = static_cast<std::uint32_t>(kHeaderSize);
    header->mapped_size = static_cast<std::uint32_t>(kMappedSize);
    header->slot_count = kSlotCount; header->slot_size = static_cast<std::uint32_t>(kSlotSize);
    header->metric_count = kMetricCount;
    header->service_generation = 1; header->producer_generation = 1;
    header->metric_definition_generation = 1; header->subject_definition_generation = 1;
    SingleProducer producer(mapping, kMappedSize);
    const bool published = producer.isValid()
        && producer.publish(SampleState::Valid, 60'000'000, 1); // fixture-only 60 percent
    ::msync(mapping, kMappedSize, MS_SYNC);
    ::munmap(mapping, kMappedSize);
    if (!published) {
        ::shm_unlink(name.constData()); ::close(m_producerFd); m_producerFd = -1; return;
    }
    m_readerFd = ::shm_open(name.constData(), O_RDONLY | O_CLOEXEC, 0);
    ::shm_unlink(name.constData());
    if (m_readerFd < 0) { ::close(m_producerFd); m_producerFd = -1; }
}
FixtureMock::~FixtureMock() {
    if (m_readerFd >= 0) ::close(m_readerFd);
    if (m_producerFd >= 0) ::close(m_producerFd);
}
OpenResult FixtureMock::openResult() const {
    return {QStringLiteral("OK"), {}, m_instance, 1, 1, 1, 1, kMajor, kMappedSize};
}
QDBusUnixFileDescriptor FixtureMock::readOnlyHandle() const {
    const int transferred = ::fcntl(m_readerFd, F_DUPFD_CLOEXEC, 0);
    if (transferred < 0) return {};
    QDBusUnixFileDescriptor descriptor;
    descriptor.giveFileDescriptor(transferred);
    return descriptor;
}
QList<MetricDefinition> FixtureMock::metrics() const {
    return {{QStringLiteral("gpu.fixture.utilization"), QStringLiteral("UNSIGNED_MICRO_UNITS"),
             QStringLiteral("percent"), QStringLiteral("VALID|UNAVAILABLE|STALE")}};
}
QList<SubjectDefinition> FixtureMock::subjects() const {
    return {{QStringLiteral("GPU_PCI"), QStringLiteral("gpu.pci.test-fixture"),
             QStringLiteral("Test fixture GPU"), QStringLiteral("test-only-opaque")}};
}
}
