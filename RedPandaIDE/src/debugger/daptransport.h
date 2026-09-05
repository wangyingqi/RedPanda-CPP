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
#ifndef DAP_TRANSPORT_H
#define DAP_TRANSPORT_H

#include <QByteArray>
#include <QJsonObject>

/**
 * Debug Adapter Protocol base-protocol framing.
 *
 * Messages are "Content-Length: N\r\n\r\n" followed by N bytes of JSON.
 * feed() accumulates raw bytes; next() pops one complete message at a time.
 */
class DAPTransport {
public:
    void feed(const QByteArray& data);
    bool next(QJsonObject& out);
    void clear();
    static QByteArray encode(const QJsonObject& message);
private:
    QByteArray mBuffer;
};

#endif // DAP_TRANSPORT_H
