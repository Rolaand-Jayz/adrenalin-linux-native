#include "pci_gpu_subject_identity.h"

#include <QCryptographicHash>

namespace adrenalin::hardware {
namespace {

constexpr auto kAmdVendorId = std::uint16_t{0x1002};
constexpr auto kNamespacePrefix = "adrenalin-linux:subject:GPU_PCI:v1";

bool isCanonicalBdf(const QString &bdf)
{
    // Require the exact lowercase, zero-padded form emitted by the libdrm
    // inventory adapter: domain:bus:device.function.
    if (bdf.size() != 12 || bdf.at(4) != QLatin1Char(':')
        || bdf.at(7) != QLatin1Char(':') || bdf.at(10) != QLatin1Char('.')) {
        return false;
    }
    for (qsizetype index = 0; index < bdf.size(); ++index) {
        if (index == 4 || index == 7 || index == 10) {
            continue;
        }
        const QChar character = bdf.at(index);
        if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9'))
              || (character >= QLatin1Char('a') && character <= QLatin1Char('f')))) {
            return false;
        }
    }
    // Slot and function fields are range-limited by PCI configuration space.
    const auto slot = bdf.mid(8, 2).toUInt(nullptr, 16);
    const auto function = bdf.mid(11, 1).toUInt(nullptr, 16);
    return slot <= 0x1f && function <= 0x7;
}

} // namespace

std::optional<GpuSubjectIdentity> gpuSubjectIdentity(const PciGpuIdentity &identity)
{
    if (identity.vendorId != kAmdVendorId || identity.deviceId == 0
        || !isCanonicalBdf(identity.canonicalBdf)) {
        return std::nullopt;
    }

    // Fixed-width numeric encodings and explicit labels make the hashed input
    // unambiguous. SHA-256 output is opaque at the interface; it is not a
    // secrecy boundary because the input space can be enumerated offline.
    const QString material = QStringLiteral("%1|vendor=%2|device=%3|bdf=%4")
        .arg(QString::fromLatin1(kNamespacePrefix))
        .arg(identity.vendorId, 4, 16, QLatin1Char('0'))
        .arg(identity.deviceId, 4, 16, QLatin1Char('0'))
        .arg(identity.canonicalBdf);
    const QByteArray digest = QCryptographicHash::hash(material.toUtf8(),
                                                        QCryptographicHash::Sha256)
                                  .toHex();

    GpuSubjectIdentity result;
    result.subjectId = QStringLiteral("gpu.pci.v1.%1")
                           .arg(QString::fromLatin1(digest));
    return result;
}

} // namespace adrenalin::hardware
