#include "translator.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace
{

class TranslatorLookupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_tempDir.isValid());
        m_databasePath = m_tempDir.filePath(QStringLiteral("ecdict-test.sqlite"));

        const QString connectionName = QStringLiteral("translator-test-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(m_databasePath);
            ASSERT_TRUE(database.open()) << database.lastError().text().toStdString();

            QSqlQuery query(database);
            ASSERT_TRUE(query.exec(QStringLiteral(
                "CREATE TABLE entries ("
                "word TEXT PRIMARY KEY, "
                "sw TEXT, "
                "phonetic TEXT, "
                "definition TEXT, "
                "translation TEXT, "
                "bnc INTEGER, "
                "frq INTEGER, "
                "exchange TEXT)"))) << query.lastError().text().toStdString();

            insertEntry(database,
                        QStringLiteral("developer"),
                        QStringLiteral("developer"),
                        QStringLiteral("dɪˈveləpər"),
                        QStringLiteral("n. a person who writes software"),
                        QStringLiteral("n. 开发者"),
                        100,
                        100,
                        QStringLiteral("s:developers"));
            insertEntry(database,
                        QStringLiteral("developers"),
                        QStringLiteral("developers"),
                        QString(),
                        QStringLiteral("n. plural of developer"),
                        QStringLiteral("n. 开发者们"),
                        200,
                        200,
                        QStringLiteral("0:developer/1:s"));
            insertEntry(database,
                        QStringLiteral("tool"),
                        QStringLiteral("tool"),
                        QStringLiteral("tuːl"),
                        QStringLiteral("n. an instrument used to do work"),
                        QStringLiteral("n. 工具"),
                        80,
                        80,
                        QStringLiteral("s:tools"));
            insertEntry(database,
                        QStringLiteral("write"),
                        QStringLiteral("write"),
                        QStringLiteral("raɪt"),
                        QStringLiteral("v. mark letters or words"),
                        QStringLiteral("v. 写"),
                        70,
                        70,
                        QStringLiteral("i:writing/p:wrote/d:written"));

            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        m_translator.setDatabaseUrl(QUrl::fromLocalFile(m_databasePath));
        m_translator.setDateReplacementMaximumLength(64);
        m_translator.setMaximumSelectionLength(128);
    }

    static void insertEntry(QSqlDatabase &database,
                            const QString &word,
                            const QString &sw,
                            const QString &phonetic,
                            const QString &definition,
                            const QString &translation,
                            int bnc,
                            int frq,
                            const QString &exchange)
    {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO entries (word, sw, phonetic, definition, translation, bnc, frq, exchange) "
            "VALUES (:word, :sw, :phonetic, :definition, :translation, :bnc, :frq, :exchange)"));
        query.bindValue(QStringLiteral(":word"), word);
        query.bindValue(QStringLiteral(":sw"), sw);
        query.bindValue(QStringLiteral(":phonetic"), phonetic);
        query.bindValue(QStringLiteral(":definition"), definition);
        query.bindValue(QStringLiteral(":translation"), translation);
        query.bindValue(QStringLiteral(":bnc"), bnc);
        query.bindValue(QStringLiteral(":frq"), frq);
        query.bindValue(QStringLiteral(":exchange"), exchange);
        ASSERT_TRUE(query.exec()) << query.lastError().text().toStdString();
    }

    QTemporaryDir m_tempDir;
    QString m_databasePath;
    Translator m_translator;
};

TEST_F(TranslatorLookupTest, LooksUpSingleSelectedWord)
{
    const QVariantMap entry = m_translator.lookup(QStringLiteral(" developer "));

    EXPECT_EQ(entry.value(QStringLiteral("matchedWord")).toString(), QStringLiteral("developer"));
    EXPECT_EQ(entry.value(QStringLiteral("summary")).toString(), QStringLiteral("开发者"));
    EXPECT_EQ(entry.value(QStringLiteral("phonetic")).toString(), QStringLiteral("dɪˈveləpər"));
    EXPECT_FALSE(entry.value(QStringLiteral("isSplitEntry")).toBool());
}

TEST_F(TranslatorLookupTest, DropsAsciiSingularPossessiveBeforeLookup)
{
    const QVariantMap entry = m_translator.lookup(QStringLiteral("Developer's"));

    EXPECT_EQ(entry.value(QStringLiteral("matchedWord")).toString(), QStringLiteral("developer"));
    EXPECT_EQ(entry.value(QStringLiteral("translation")).toString(), QStringLiteral("n. 开发者"));
}

TEST_F(TranslatorLookupTest, DropsCurlySingularPossessiveBeforeLookup)
{
    const QVariantMap entry = m_translator.lookup(QStringLiteral("Developer’s"));

    EXPECT_EQ(entry.value(QStringLiteral("matchedWord")).toString(), QStringLiteral("developer"));
    EXPECT_EQ(entry.value(QStringLiteral("translation")).toString(), QStringLiteral("n. 开发者"));
}

TEST_F(TranslatorLookupTest, KeepsRealPluralLookupAvailable)
{
    const QVariantMap entry = m_translator.lookup(QStringLiteral("developers"));

    EXPECT_EQ(entry.value(QStringLiteral("matchedWord")).toString(), QStringLiteral("developers"));
    EXPECT_EQ(entry.value(QStringLiteral("translation")).toString(), QStringLiteral("n. 开发者们"));
}

TEST_F(TranslatorLookupTest, SplitsSelectionAndNormalizesPossessiveWords)
{
    const QVariantMap entry = m_translator.lookup(QStringLiteral("Developer’s tool"));
    const QVariantList popupEntries = entry.value(QStringLiteral("popupEntries")).toList();

    ASSERT_TRUE(entry.value(QStringLiteral("isSplitEntry")).toBool());
    ASSERT_EQ(popupEntries.size(), 2);
    EXPECT_EQ(popupEntries.at(0).toMap().value(QStringLiteral("matchedWord")).toString(), QStringLiteral("developer"));
    EXPECT_EQ(popupEntries.at(1).toMap().value(QStringLiteral("matchedWord")).toString(), QStringLiteral("tool"));
    EXPECT_EQ(entry.value(QStringLiteral("summary")).toString(), QStringLiteral("开发者 工具"));
}

TEST_F(TranslatorLookupTest, UsesCandidateWordsForInflectedSelection)
{
    const QVariantMap entry = m_translator.lookup(QStringLiteral("writing"));

    EXPECT_EQ(entry.value(QStringLiteral("matchedWord")).toString(), QStringLiteral("write"));
    EXPECT_EQ(entry.value(QStringLiteral("summary")).toString(), QStringLiteral("写"));
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
