/*
 * Copyright (C) 2020-2026 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "daptransport.h"
#include <QJsonDocument>

static const QByteArray HEADER_END("\r\n\r\n");
static const QByteArray CONTENT_LENGTH("Content-Length:");

void DAPTransport::feed(const QByteArray &data)
{
    mBuffer.append(data);
}

void DAPTransport::clear()
{
    mBuffer.clear();
}

bool DAPTransport::next(QJsonObject &out)
{
    while (true) {
        int headerEnd = mBuffer.indexOf(HEADER_END);
        if (headerEnd < 0)
            return false;
        int contentLength = -1;
        const QList<QByteArray> headerLines = mBuffer.left(headerEnd).split('\n');
        for (const QByteArray& rawLine : headerLines) {
            QByteArray line = rawLine.trimmed();
            if (line.startsWith(CONTENT_LENGTH)) {
                bool ok;
                contentLength = line.mid(CONTENT_LENGTH.length()).trimmed().toInt(&ok);
                if (!ok)
                    contentLength = -1;
            }
        }
        int bodyStart = headerEnd + HEADER_END.length();
        if (contentLength < 0) {
            // malformed header: drop it and keep scanning
            mBuffer.remove(0, bodyStart);
            continue;
        }
        if (mBuffer.length() < bodyStart + contentLength)
            return false;
        QByteArray body = mBuffer.mid(bodyStart, contentLength);
        mBuffer.remove(0, bodyStart + contentLength);
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            continue; // skip unparsable message
        out = doc.object();
        return true;
    }
}

QByteArray DAPTransport::encode(const QJsonObject &message)
{
    QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray result = CONTENT_LENGTH + " " + QByteArray::number(body.length()) + HEADER_END;
    result.append(body);
    return result;
}
