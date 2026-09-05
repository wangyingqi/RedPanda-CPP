#include <QtTest>
#include "debugger/daptransport.h"

class TestDAPTransport : public QObject {
    Q_OBJECT
private slots:
    void singleFrame() {
        DAPTransport t;
        t.feed("Content-Length: 26\r\n\r\n{\"seq\":1,\"type\":\"request\"}");
        QJsonObject obj;
        QVERIFY(t.next(obj));
        QCOMPARE(obj["seq"].toInt(), 1);
        QCOMPARE(obj["type"].toString(), QString("request"));
        QVERIFY(!t.next(obj));
    }
    void twoFramesInOneChunk() {
        DAPTransport t;
        QByteArray a = DAPTransport::encode(QJsonObject{{"seq", 1}, {"type", "event"}});
        QByteArray b = DAPTransport::encode(QJsonObject{{"seq", 2}, {"type", "response"}});
        t.feed(a + b);
        QJsonObject obj;
        QVERIFY(t.next(obj)); QCOMPARE(obj["seq"].toInt(), 1);
        QVERIFY(t.next(obj)); QCOMPARE(obj["seq"].toInt(), 2);
        QVERIFY(!t.next(obj));
    }
    void partialFrameWaits() {
        DAPTransport t;
        QByteArray full = DAPTransport::encode(QJsonObject{{"seq", 7}, {"type", "event"}});
        t.feed(full.left(10));
        QJsonObject obj;
        QVERIFY(!t.next(obj));
        t.feed(full.mid(10, 15));
        QVERIFY(!t.next(obj));
        t.feed(full.mid(25));
        QVERIFY(t.next(obj));
        QCOMPARE(obj["seq"].toInt(), 7);
    }
    void encodeHasCorrectLength() {
        QByteArray msg = DAPTransport::encode(QJsonObject{{"seq", 1}, {"type", "request"}, {"command", "initialize"}});
        int headerEnd = msg.indexOf("\r\n\r\n");
        QVERIFY(headerEnd > 0);
        QByteArray header = msg.left(headerEnd);
        QByteArray body = msg.mid(headerEnd + 4);
        QCOMPARE(header, QByteArray("Content-Length: ") + QByteArray::number(body.length()));
        QVERIFY(body.startsWith('{'));
    }
    void extraHeadersAndUnicode() {
        DAPTransport t;
        QJsonObject src{{"seq", 3}, {"type", "event"}, {"body", QJsonObject{{"output", QString::fromUtf8("和 = 7\n")}}}};
        QByteArray enc = DAPTransport::encode(src);
        enc.replace("\r\n\r\n", "\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n");
        t.feed(enc);
        QJsonObject obj;
        QVERIFY(t.next(obj));
        QCOMPARE(obj["body"].toObject()["output"].toString(), QString::fromUtf8("和 = 7\n"));
    }
};

QTEST_GUILESS_MAIN(TestDAPTransport)
#include "test_daptransport.moc"
