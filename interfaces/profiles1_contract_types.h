#pragma once

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QVariantMap>

namespace adrenalin::contracts::profiles1 {

// subjectKind/subjectId identify the profile owner, not a localized label.
// The global owner is ("GLOBAL", "global"); game owners use source-qualified
// stable game IDs. Preset and Custom semantics remain reference-gated.
struct Profile final {
    QString profileId;
    QString subjectKind;
    QString subjectId;
    // Reserved for a future reference-backed contract; must remain empty here.
    QString presetId;
    QString referenceState;
    quint64 revision = 0;
    QVariantMap settings;

    bool isValid(QString *error = nullptr) const;
};

struct ReadReply final {
    QString code;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    Profile profile;

    bool isValid(QString *error = nullptr) const;
    bool isValidFor(const QString &subjectKind, const QString &subjectId,
                    QString *error = nullptr) const;
};

bool isValidSubject(const QString &subjectKind, const QString &subjectId);
bool isValidSettings(const QVariantMap &settings, QString *error = nullptr);
void registerMetaTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Profile &profile);
const QDBusArgument &operator>>(const QDBusArgument &argument, Profile &profile);

} // namespace adrenalin::contracts::profiles1

Q_DECLARE_METATYPE(adrenalin::contracts::profiles1::Profile)
