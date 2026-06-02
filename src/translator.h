#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSqlDatabase>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkReply;

class Translator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl databaseUrl READ databaseUrl WRITE setDatabaseUrl NOTIFY databaseUrlChanged)
    Q_PROPERTY(int maximumSelectionLength READ maximumSelectionLength WRITE setMaximumSelectionLength NOTIFY maximumSelectionLengthChanged)
    Q_PROPERTY(
        int dateReplacementMaximumLength READ dateReplacementMaximumLength WRITE
            setDateReplacementMaximumLength NOTIFY
                dateReplacementMaximumLengthChanged)
    QML_ELEMENT

public:
    explicit Translator(QObject *parent = nullptr);
    ~Translator() override;

    QUrl databaseUrl() const;
    void setDatabaseUrl(const QUrl &databaseUrl);

    int maximumSelectionLength() const;
    void setMaximumSelectionLength(int maximumSelectionLength);

    int dateReplacementMaximumLength() const;
    void setDateReplacementMaximumLength(int dateReplacementMaximumLength);

    Q_INVOKABLE QVariantMap lookup(const QString &selectedText);
    Q_INVOKABLE void translateSentenceWithDeepSeek(const QString &selectedText,
                                                   const QString &apiKey);
    Q_INVOKABLE void cancelSentenceTranslation();

  Q_SIGNALS:
    void databaseUrlChanged();
    void maximumSelectionLengthChanged();
    void dateReplacementMaximumLengthChanged();
    void sentenceTranslationReady(const QString &selectedText,
                                  const QString &translation);
    void sentenceTranslationFailed(const QString &selectedText,
                                   const QString &message);

  private:
    bool ensureDatabase();
    QString normalizeQuery(QString text) const;
    QString normalizeSentenceQuery(QString text) const;
    QString strippedWord(QString text) const;
    QStringList candidateWords(const QString &word) const;
    QVariantMap lookupCandidate(const QString &word, const QString &sw);
    QVariantMap lookupSplitWords(const QString &query);
    QVariantMap buildEntry(const QString &query, const QString &word, const QString &phonetic, const QString &definition, const QString &translation, const QString &exchange) const;
    QVariantMap buildSplitEntry(const QString &query, const QList<QVariantMap> &entries) const;
    QString limitedQueryRespectingWordBoundary(const QString &query) const;
    QStringList splitQueryWords(const QString &query) const;
    QString compactSummary(const QString &translation, const QString &definition) const;
    QString cleanDateReplacementSummary(QString text) const;
    QString truncateDateReplacement(QString text) const;
    QVariantMap withTruncationNote(QVariantMap entry,
                                   const QString &note) const;
    QString firstTranslationFragment(const QString &translation) const;
    QString fullText(const QString &word, const QString &phonetic, const QString &definition, const QString &translation, const QString &exchange) const;
    QVariantList partOfSpeechRows(const QString &text) const;
    QString takeLeadingDomainTags(QString *text) const;
    QVariantList exchangeRows(const QString &exchange) const;
    QString exchangeLabel(const QString &key) const;
    QString normalizeDictionaryText(QString text) const;
    QString swKey(QString text) const;

    QUrl m_databaseUrl;
    QString m_connectionName;
    QSqlDatabase m_database;
    QNetworkAccessManager m_networkAccessManager;
    QPointer<QNetworkReply> m_sentenceReply;
    QString m_pendingSentence;
    int m_maximumSelectionLength = 128;
    int m_dateReplacementMaximumLength = 10;
};
