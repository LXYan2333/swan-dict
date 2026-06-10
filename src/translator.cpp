#include "translator.h"

#include <algorithm>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <KLocalizedString>

Translator::Translator(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("swan-dict-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

Translator::~Translator()
{
    cancelSentenceTranslation();

    if (m_database.isValid()) {
        m_database.close();
    }
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

QUrl Translator::databaseUrl() const
{
    return m_databaseUrl;
}

void Translator::setDatabaseUrl(const QUrl &databaseUrl)
{
    if (m_databaseUrl == databaseUrl) {
        return;
    }

    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    m_databaseUrl = databaseUrl;
    Q_EMIT databaseUrlChanged();
}

int Translator::maximumSelectionLength() const
{
    return m_maximumSelectionLength;
}

void Translator::setMaximumSelectionLength(int maximumSelectionLength)
{
    const int boundedLength = std::max(1, maximumSelectionLength);
    if (m_maximumSelectionLength == boundedLength) {
        return;
    }

    m_maximumSelectionLength = boundedLength;
    Q_EMIT maximumSelectionLengthChanged();
}

int Translator::dateReplacementMaximumLength() const
{
    return m_dateReplacementMaximumLength;
}

void Translator::setDateReplacementMaximumLength(int dateReplacementMaximumLength)
{
    const int boundedLength = std::max(4, dateReplacementMaximumLength);
    if (m_dateReplacementMaximumLength == boundedLength) {
        return;
    }

    m_dateReplacementMaximumLength = boundedLength;
    Q_EMIT dateReplacementMaximumLengthChanged();
}

QVariantMap Translator::lookup(const QString &selectedText)
{
    const QString query = normalizeQuery(selectedText);
    if (query.isEmpty() || !ensureDatabase()) {
        return {};
    }

    const bool isTruncated = query.size() > m_maximumSelectionLength;
    const QString effectiveQuery = isTruncated ? limitedQueryRespectingWordBoundary(query) : query;
    if (effectiveQuery.isEmpty()) {
        return {};
    }

    const QString lowerQuery = effectiveQuery.toLower();
    for (const QString &candidate : candidateWords(lowerQuery)) {
        QVariantMap result = lookupCandidate(candidate, swKey(candidate));
        if (!result.isEmpty()) {
            result.insert(QStringLiteral("query"), query);
            if (!isTruncated) {
                return result;
            }

            const QString note = i18n("Selection exceeded %1 characters; translated first %2 characters.",
                                      m_maximumSelectionLength,
                                      effectiveQuery.size());
            return withTruncationNote(result, note);
        }
    }

    QVariantMap result = lookupSplitWords(effectiveQuery);
    if (result.isEmpty()) {
        return result;
    }
    result.insert(QStringLiteral("query"), query);
    if (!isTruncated) {
        return result;
    }

    const QString note = i18n("Selection exceeded %1 characters; translated first %2 characters.",
                              m_maximumSelectionLength,
                              effectiveQuery.size());
    return withTruncationNote(result, note);
}

void Translator::translateSentenceWithDeepSeek(const QString &selectedText, const QString &apiKey)
{
    const QString text = normalizeSentenceQuery(selectedText);
    const QString trimmedApiKey = apiKey.trimmed();
    if (text.isEmpty() || trimmedApiKey.isEmpty()) {
        Q_EMIT sentenceTranslationFailed(text, i18n("DeepSeek API key is empty."));
        return;
    }

    cancelSentenceTranslation();
    m_pendingSentence = text;

    QNetworkRequest request(QUrl(QStringLiteral("https://api.deepseek.com/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(trimmedApiKey).toUtf8());

    const QJsonObject payload {
        {QStringLiteral("model"), QStringLiteral("deepseek-v4-flash")},
        {QStringLiteral("stream"), false},
        {QStringLiteral("thinking"), QJsonObject {
             {QStringLiteral("type"), QStringLiteral("disabled")},
         }},
        {QStringLiteral("messages"), QJsonArray {
             QJsonObject {
                 {QStringLiteral("role"), QStringLiteral("system")},
                 {QStringLiteral("content"), QStringLiteral("Translate the user's selected English sentence or phrase into concise Simplified Chinese. Return only the translation.")},
             },
             QJsonObject {
                 {QStringLiteral("role"), QStringLiteral("user")},
                 {QStringLiteral("content"), text},
             },
         }},
    };

    QNetworkReply *reply = m_networkAccessManager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_sentenceReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_sentenceReply) {
            reply->deleteLater();
            return;
        }

        const QString selectedText = m_pendingSentence;
        m_sentenceReply = nullptr;
        m_pendingSentence.clear();

        const QByteArray body = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorString = reply->errorString();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError) {
            Q_EMIT sentenceTranslationFailed(selectedText, networkErrorString);
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject root = document.object();
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            const QString message = root.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
            Q_EMIT sentenceTranslationFailed(selectedText, message.isEmpty() ? i18n("DeepSeek returned no translation.") : message);
            return;
        }

        const QString translation = choices.first()
            .toObject()
            .value(QStringLiteral("message"))
            .toObject()
            .value(QStringLiteral("content"))
            .toString()
            .trimmed();
        if (translation.isEmpty()) {
            Q_EMIT sentenceTranslationFailed(selectedText, i18n("DeepSeek returned an empty translation."));
            return;
        }

        Q_EMIT sentenceTranslationReady(selectedText, translation);
    });
}

void Translator::cancelSentenceTranslation()
{
    if (!m_sentenceReply) {
        return;
    }

    QNetworkReply *reply = m_sentenceReply;
    m_sentenceReply = nullptr;
    m_pendingSentence.clear();
    reply->abort();
}

bool Translator::ensureDatabase()
{
    if (m_database.isValid() && m_database.isOpen()) {
        return true;
    }

    const QString path = m_databaseUrl.isLocalFile() ? m_databaseUrl.toLocalFile() : m_databaseUrl.toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(path);
    return m_database.open();
}

QString Translator::normalizeQuery(QString text) const
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QString();
    }

    if (text.contains(u'\n') || text.contains(u'\r')) {
        return QString();
    }

    return strippedWord(removePossessiveSuffixes(text));
}

QString Translator::normalizeSentenceQuery(QString text) const
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QString();
    }

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return text.replace(whitespace, QStringLiteral(" "));
}

QString Translator::strippedWord(QString text) const
{
    static const QRegularExpression edgePunctuation(QStringLiteral("^[\\s\\p{P}\\p{S}]+|[\\s\\p{P}\\p{S}]+$"));
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    text = text.replace(edgePunctuation, QString());
    return text.replace(whitespace, QStringLiteral(" ")).trimmed();
}

QString Translator::removePossessiveSuffixes(QString text) const
{
    static const QRegularExpression singularPossessive(QStringLiteral("([A-Za-z])['\u2019]s(?=$|[^A-Za-z])"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression pluralPossessive(QStringLiteral("([A-Za-z]s)['\u2019](?=$|[^A-Za-z])"), QRegularExpression::CaseInsensitiveOption);
    text.replace(singularPossessive, QStringLiteral("\\1"));
    text.replace(pluralPossessive, QStringLiteral("\\1"));
    return text;
}

QStringList Translator::candidateWords(const QString &word) const
{
    QStringList candidates;
    const auto addCandidate = [&candidates](const QString &candidate) {
        if (!candidate.isEmpty() && !candidates.contains(candidate)) {
            candidates.push_back(candidate);
        }
    };

    addCandidate(word);

    if (word.endsWith(QStringLiteral("ies")) && word.size() > 3) {
        addCandidate(word.left(word.size() - 3) + QStringLiteral("y"));
    }
    if (word.endsWith(QStringLiteral("es")) && word.size() > 2) {
        addCandidate(word.left(word.size() - 2));
    }
    if (word.endsWith(u's') && word.size() > 1) {
        addCandidate(word.left(word.size() - 1));
    }
    if (word.endsWith(QStringLiteral("ied")) && word.size() > 3) {
        addCandidate(word.left(word.size() - 3) + QStringLiteral("y"));
    }
    if (word.endsWith(QStringLiteral("ed")) && word.size() > 2) {
        addCandidate(word.left(word.size() - 2));
    }
    if (word.endsWith(QStringLiteral("ing")) && word.size() > 4) {
        addCandidate(word.left(word.size() - 3));
        addCandidate(word.left(word.size() - 3) + QStringLiteral("e"));
    }

    return candidates;
}

QVariantMap Translator::lookupCandidate(const QString &word, const QString &sw)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT word, phonetic, definition, translation, exchange "
        "FROM entries "
        "WHERE word = :word OR sw = :sw "
        "ORDER BY CASE WHEN word = :word THEN 0 ELSE 1 END, frq DESC, bnc DESC "
        "LIMIT 1"));
    query.bindValue(QStringLiteral(":word"), word);
    query.bindValue(QStringLiteral(":sw"), sw);

    if (!query.exec() || !query.next()) {
        return {};
    }

    return buildEntry(
        word,
        query.value(0).toString(),
        query.value(1).toString(),
        query.value(2).toString(),
        query.value(3).toString(),
        query.value(4).toString());
}

QVariantMap Translator::lookupSplitWords(const QString &query)
{
    const QStringList words = splitQueryWords(query);
    if (words.size() < 2) {
        return {};
    }

    QList<QVariantMap> entries;
    for (const QString &word : words) {
        for (const QString &candidate : candidateWords(word.toLower())) {
            QVariantMap entry = lookupCandidate(candidate, swKey(candidate));
            if (!entry.isEmpty()) {
                entries.push_back(entry);
                break;
            }
        }
    }

    if (entries.isEmpty()) {
        return {};
    }

    return buildSplitEntry(query, entries);
}

QVariantMap Translator::buildEntry(const QString &query, const QString &word, const QString &phonetic, const QString &definition, const QString &translation, const QString &exchange) const
{
    const QString normalizedTranslation = normalizeDictionaryText(translation).trimmed();
    const QString normalizedDefinition = normalizeDictionaryText(definition).trimmed();
    const QString normalizedExchange = normalizeDictionaryText(exchange).trimmed();
    const QString summary = compactSummary(normalizedTranslation, normalizedDefinition);
    const QString full = fullText(word, phonetic, normalizedDefinition, normalizedTranslation, normalizedExchange);
    const QVariantList parsedTranslationRows = partOfSpeechRows(normalizedTranslation);
    const QVariantList parsedDefinitionRows = partOfSpeechRows(normalizedDefinition);
    const QVariantList parsedExchangeRows = exchangeRows(normalizedExchange);
    if (summary.isEmpty() && full.isEmpty()) {
        return {};
    }

    return {
        {QStringLiteral("query"), query},
        {QStringLiteral("matchedWord"), word},
        {QStringLiteral("summary"), summary},
        {QStringLiteral("fullText"), full},
        {QStringLiteral("phonetic"), phonetic},
        {QStringLiteral("translation"), normalizedTranslation},
        {QStringLiteral("translationRows"), parsedTranslationRows},
        {QStringLiteral("definition"), normalizedDefinition},
        {QStringLiteral("definitionRows"), parsedDefinitionRows},
        {QStringLiteral("exchange"), normalizedExchange},
        {QStringLiteral("exchangeRows"), parsedExchangeRows},
        {QStringLiteral("isSplitEntry"), false},
        {QStringLiteral("popupEntries"), QVariantList()},
    };
}

QVariantMap Translator::buildSplitEntry(const QString &query, const QList<QVariantMap> &entries) const
{
    QStringList summaryParts;
    QStringList fullParts;
    QVariantList briefRows;
    QVariantList popupEntries;

    for (const QVariantMap &entry : entries) {
        const QString word = entry.value(QStringLiteral("matchedWord")).toString();
        QString summary = firstTranslationFragment(entry.value(QStringLiteral("translation")).toString());
        if (word.isEmpty()) {
            continue;
        }

        const QVariantList translationRows = entry.value(QStringLiteral("translationRows")).toList();
        if (summary.isEmpty()) {
            summary = entry.value(QStringLiteral("summary")).toString();
        }

        const QString dateSummary = cleanDateReplacementSummary(summary);
        if (!dateSummary.isEmpty()) {
            summaryParts << dateSummary;
        }

        if (!translationRows.isEmpty()) {
            bool isFirstRow = true;
            bool isFirstWord = popupEntries.isEmpty();
            for (const QVariant &rowValue : translationRows) {
                QVariantMap row = rowValue.toMap();
                if (isFirstRow) {
                    row.insert(QStringLiteral("word"), word);
                    row.insert(QStringLiteral("isFirstWord"), isFirstWord);
                    isFirstRow = false;
                }
                briefRows.push_back(row);
            }
        } else if (!summary.isEmpty()) {
            briefRows.push_back(QVariantMap {
                {QStringLiteral("word"), word},
                {QStringLiteral("isFirstWord"), popupEntries.isEmpty()},
                {QStringLiteral("pos"), QString()},
                {QStringLiteral("text"), summary},
            });
        }
        fullParts << QStringLiteral("%1: %2").arg(word, entry.value(QStringLiteral("summary")).toString());
        popupEntries << entry;
    }

    if (summaryParts.isEmpty() || popupEntries.isEmpty()) {
        return {};
    }

    const QString summary = truncateDateReplacement(summaryParts.join(QStringLiteral(" ")));

    const QString full = fullParts.join(QStringLiteral("\n"));
    return {
        {QStringLiteral("query"), query},
        {QStringLiteral("matchedWord"), query},
        {QStringLiteral("summary"), summary},
        {QStringLiteral("fullText"), full},
        {QStringLiteral("phonetic"), QString()},
        {QStringLiteral("translation"), full},
        {QStringLiteral("translationRows"), briefRows},
        {QStringLiteral("definition"), QString()},
        {QStringLiteral("definitionRows"), QVariantList()},
        {QStringLiteral("exchange"), QString()},
        {QStringLiteral("exchangeRows"), QVariantList()},
        {QStringLiteral("isSplitEntry"), true},
        {QStringLiteral("popupEntries"), popupEntries},
    };
}

QString Translator::limitedQueryRespectingWordBoundary(const QString &query) const
{
    if (query.size() <= m_maximumSelectionLength) {
        return query;
    }

    qsizetype end = m_maximumSelectionLength;
    while (end < query.size() && query.at(end).isLetterOrNumber()) {
        ++end;
    }

    return strippedWord(query.left(end));
}

QStringList Translator::splitQueryWords(const QString &query) const
{
    QString text = removePossessiveSuffixes(query);
    text.replace(QRegularExpression(QStringLiteral("([A-Z]+)([A-Z][a-z])")), QStringLiteral("\\1 \\2"));
    text.replace(QRegularExpression(QStringLiteral("([a-z0-9])([A-Z])")), QStringLiteral("\\1 \\2"));
    text.replace(QRegularExpression(QStringLiteral("([A-Za-z])([0-9])")), QStringLiteral("\\1 \\2"));
    text.replace(QRegularExpression(QStringLiteral("([0-9])([A-Za-z])")), QStringLiteral("\\1 \\2"));
    text.replace(QRegularExpression(QStringLiteral("[^A-Za-z]+")), QStringLiteral(" "));

    QStringList words;
    const QStringList rawWords = text.split(u' ', Qt::SkipEmptyParts);
    for (QString word : rawWords) {
        word = strippedWord(word).toLower();
        if (word.size() < 2) {
            continue;
        }
        if (!words.contains(word)) {
            words.push_back(word);
        }
    }

    return words;
}

QString Translator::compactSummary(const QString &translation, const QString &definition) const
{
    QString text = normalizeDictionaryText(translation).trimmed();
    if (text.isEmpty()) {
        text = normalizeDictionaryText(definition).trimmed();
    }

    const int newline = text.indexOf(u'\n');
    if (newline >= 0) {
        text = text.left(newline);
    }

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    text = text.replace(whitespace, QStringLiteral(" ")).trimmed();

    return truncateDateReplacement(cleanDateReplacementSummary(text));
}

QString Translator::cleanDateReplacementSummary(QString text) const
{
    static const QRegularExpression leadingPartOfSpeech(QStringLiteral("^[A-Za-z]+\\.\\s*"));
    static const QRegularExpression leadingDomainTags(QStringLiteral("^(?:\\[[^\\]]+\\]\\s*)+"));
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));

    text = text.replace(leadingDomainTags, QString());
    text = text.replace(leadingPartOfSpeech, QString());
    text = text.replace(leadingDomainTags, QString());
    return text.replace(whitespace, QStringLiteral(" ")).trimmed();
}

QString Translator::truncateDateReplacement(QString text) const
{
    text = text.trimmed();
    if (text.size() <= m_dateReplacementMaximumLength) {
        return text;
    }

    return text.left(m_dateReplacementMaximumLength - 1).trimmed() + QStringLiteral("…");
}

QVariantMap Translator::withTruncationNote(QVariantMap entry, const QString &note) const
{
    if (note.isEmpty()) {
        return entry;
    }

    entry.insert(QStringLiteral("isSelectionTruncated"), true);
    entry.insert(QStringLiteral("truncationNote"), note);

    QString fullText = entry.value(QStringLiteral("fullText")).toString();
    fullText = fullText.isEmpty() ? note : fullText + QStringLiteral("\n\n") + note;
    entry.insert(QStringLiteral("fullText"), fullText);

    QString translation = entry.value(QStringLiteral("translation")).toString();
    translation = translation.isEmpty() ? note : translation + QStringLiteral("\n") + note;
    entry.insert(QStringLiteral("translation"), translation);

    QVariantList translationRows = entry.value(QStringLiteral("translationRows")).toList();
    translationRows.push_back(QVariantMap {
        {QStringLiteral("pos"), QString()},
        {QStringLiteral("text"), note},
        {QStringLiteral("isWarning"), true},
    });
    entry.insert(QStringLiteral("translationRows"), translationRows);

    return entry;
}

QString Translator::firstTranslationFragment(const QString &translation) const
{
    QString text = normalizeDictionaryText(translation).trimmed();
    if (text.isEmpty()) {
        return QString();
    }

    const int newline = text.indexOf(u'\n');
    if (newline >= 0) {
        text = text.left(newline);
    }

    static const QRegularExpression leadingPartOfSpeech(QStringLiteral("^[a-zA-Z.\\s]+\\s+"));
    text = text.replace(leadingPartOfSpeech, QString());

    const QList<QString> separators = {
        QStringLiteral(";"),
        QStringLiteral("；"),
        QStringLiteral(","),
        QStringLiteral("，"),
        QStringLiteral("、"),
    };
    for (const QString &separator : separators) {
        const int index = text.indexOf(separator);
        if (index > 0) {
            text = text.left(index);
        }
    }

    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return text.replace(whitespace, QStringLiteral(" ")).trimmed();
}

QString Translator::fullText(const QString &word, const QString &phonetic, const QString &definition, const QString &translation, const QString &exchange) const
{
    QStringList parts;
    if (!word.trimmed().isEmpty()) {
        parts << word.trimmed();
    }
    if (!phonetic.trimmed().isEmpty()) {
        parts << QStringLiteral("/%1/").arg(phonetic.trimmed());
    }
    const QString normalizedTranslation = normalizeDictionaryText(translation).trimmed();
    if (!normalizedTranslation.isEmpty()) {
        parts << normalizedTranslation;
    }
    const QString normalizedDefinition = normalizeDictionaryText(definition).trimmed();
    if (!normalizedDefinition.isEmpty()) {
        parts << normalizedDefinition;
    }
    return parts.join(QStringLiteral("\n\n"));
}

QVariantList Translator::partOfSpeechRows(const QString &text) const
{
    QVariantList rows;
    const QString normalizedText = normalizeDictionaryText(text).trimmed();
    if (normalizedText.isEmpty()) {
        return rows;
    }

    static const QString posMarkers = QStringLiteral("(?:n|v|a|s|r|adj|adv|vi|vt|prep|pron|conj|interj|int|num|aux|abbr|art|pl|sing|suf|suff|pref|prefix|comb|phrase|phr|idiom|p|imp|un|na|alt|vbl|pla|st|superl|pp|obs|adjective|pret|dv|pn|vb|exclam|obj|quant|noun|compar|ads|ad|ind|col|ph|ing|verb|fem|imperative|pr|usu|indef|dat)");
    static const QRegularExpression markerBoundary(QStringLiteral("\\s+(?=(?:%1\\.\\s|\\[[^\\]]+\\]))").arg(posMarkers), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rowPattern(QStringLiteral("^(%1\\.)\\s*(.*)$").arg(posMarkers), QRegularExpression::CaseInsensitiveOption);
    const QString preparedText = QString(normalizedText).replace(markerBoundary, QStringLiteral("\n"));
    const QStringList lines = preparedText.split(u'\n', Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QRegularExpressionMatch match = rowPattern.match(line);
        QString pos;
        QString body = line;
        if (match.hasMatch()) {
            pos = match.captured(1).trimmed();
            body = match.captured(2).trimmed();
        }

        const QString domainTags = takeLeadingDomainTags(&body);
        if (pos.isEmpty()) {
            pos = domainTags;
        }

        rows.push_back(QVariantMap {
            {QStringLiteral("pos"), pos},
            {QStringLiteral("text"), body},
        });
    }

    return rows;
}

QString Translator::takeLeadingDomainTags(QString *text) const
{
    if (!text) {
        return QString();
    }

    QString body = text->trimmed();
    static const QRegularExpression leadingDomainTags(QStringLiteral("^(?:\\[([^\\]]+)\\]\\s*)+"));
    static const QRegularExpression domainTag(QStringLiteral("\\[([^\\]]+)\\]"));
    const QRegularExpressionMatch match = leadingDomainTags.match(body);
    if (!match.hasMatch()) {
        *text = body;
        return QString();
    }

    QStringList tags;
    const QString matchedTags = match.captured(0);
    QRegularExpressionMatchIterator iterator = domainTag.globalMatch(matchedTags);
    while (iterator.hasNext()) {
        const QString tag = iterator.next().captured(1).trimmed();
        if (!tag.isEmpty()) {
            tags << tag;
        }
    }

    body = body.mid(match.capturedEnd()).trimmed();
    *text = body;
    return tags.join(QStringLiteral(" "));
}

QVariantList Translator::exchangeRows(const QString &exchange) const
{
    QVariantList rows;
    const QString normalizedExchange = normalizeDictionaryText(exchange).trimmed();
    if (normalizedExchange.isEmpty()) {
        return rows;
    }

    const QStringList fields = normalizedExchange.split(u'/', Qt::SkipEmptyParts);
    for (const QString &field : fields) {
        const int separator = field.indexOf(u':');
        if (separator <= 0 || separator >= field.size() - 1) {
            continue;
        }

        const QString key = field.left(separator).trimmed();
        const QString value = field.mid(separator + 1).trimmed();
        const QString label = exchangeLabel(key);
        if (label.isEmpty() || value.isEmpty()) {
            continue;
        }

        rows.push_back(QVariantMap {
            {QStringLiteral("key"), key},
            {QStringLiteral("label"), label},
            {QStringLiteral("value"), value},
        });
    }

    return rows;
}

QString Translator::exchangeLabel(const QString &key) const
{
    if (key == QStringLiteral("p")) {
        return QStringLiteral("过去式（did）");
    }
    if (key == QStringLiteral("d")) {
        return QStringLiteral("过去分词（done）");
    }
    if (key == QStringLiteral("i")) {
        return QStringLiteral("现在分词（doing）");
    }
    if (key == QStringLiteral("3")) {
        return QStringLiteral("第三人称单数（does）");
    }
    if (key == QStringLiteral("r")) {
        return QStringLiteral("形容词比较级（-er）");
    }
    if (key == QStringLiteral("t")) {
        return QStringLiteral("形容词最高级（-est）");
    }
    if (key == QStringLiteral("s")) {
        return QStringLiteral("名词复数形式");
    }
    if (key == QStringLiteral("0")) {
        return QStringLiteral("Lemma");
    }
    if (key == QStringLiteral("1")) {
        return QStringLiteral("Lemma 的变换形式");
    }
    return QString();
}

QString Translator::normalizeDictionaryText(QString text) const
{
    text.replace(QStringLiteral("\\r\\n"), QStringLiteral("\n"));
    text.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    text.replace(QStringLiteral("\\r"), QStringLiteral("\n"));
    text.replace(QStringLiteral("\\t"), QStringLiteral(" "));
    return text;
}

QString Translator::swKey(QString text) const
{
    text = text.toLower();
    static const QRegularExpression ignored(QStringLiteral("[^a-z0-9]+"));
    return text.replace(ignored, QString());
}
