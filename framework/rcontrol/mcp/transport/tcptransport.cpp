/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore/Audacity CLA applies
 *
 * Copyright (C) 2026 MuseScore/Audacity and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "tcptransport.h"

#include <QTcpServer>
#include <QTcpSocket>

#include "global/serialization/json.h"

#include "log.h"

static const quint16 DEFAULT_PORT = 2212;

//! NOTE MUSE_RCONTROL_PORT overrides the port, e.g. to run several instances
static quint16 port()
{
    bool ok = false;
    const int envPort = qEnvironmentVariableIntValue("MUSE_RCONTROL_PORT", &ok);
    return ok && envPort > 0 && envPort <= 65535 ? static_cast<quint16>(envPort) : DEFAULT_PORT;
}

using namespace muse::rcontrol::mcp;

TcpConnection::TcpConnection(QTcpSocket* socket, const ITransport::RequestHandler& onRequest, QObject* parent)
    : QObject(parent), m_socket(socket), m_onRequest(onRequest)
{
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &QObject::deleteLater);
}

void TcpConnection::onReadyRead()
{
    m_buffer += m_socket->readAll();

    int idx = -1;
    while ((idx = m_buffer.indexOf('\n')) != -1) {
        const QByteArray message = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        processMessage(message);
    }
}

void TcpConnection::processMessage(const QByteArray& request)
{
    LOGD() << "request: " << request;

    if (m_onRequest) {
        ByteArray req = ByteArray::fromQByteArrayNoCopy(request);
        m_onRequest(req, [this](const ByteArray& response) {
            QByteArray resp = response.toQByteArrayNoCopy();
            LOGD() << "response: " << resp;
            m_socket->write(resp);
            m_socket->write("\n");
            m_socket->flush();
        });
    } else {
        LOGE() << "No onResponse handler";
        m_socket->write("\n");
        m_socket->flush();
    }
}

TcpTransport::~TcpTransport()
{
    stop();
}

bool TcpTransport::start()
{
    if (!m_server) {
        m_server = new QTcpServer();
        QObject::connect(m_server, &QTcpServer::newConnection, [this]() {
            QTcpSocket* socket = m_server->nextPendingConnection();
            if (m_connection) {
                LOGE() << "Already connected, refusing another client";
                socket->disconnectFromHost();
                socket->deleteLater();
                return;
            }

            m_connection = new TcpConnection(socket, m_onRequest);

            //! NOTE Free the slot as soon as the client leaves, not when the
            //! connection object is eventually deleted, so a client that
            //! reconnects right away isn't refused.
            QObject::connect(socket, &QTcpSocket::disconnected, m_server, [this]() {
                m_connection.clear();
            });
        });
    }

    //! NOTE Local clients only: this channel controls the app
    if (!m_server->listen(QHostAddress::LocalHost, port())) {
        LOGE() << "TcpTransport failed to listen on port " << port() << ": " << m_server->errorString();
        return false;
    }

    LOGI() << "TcpTransport started on localhost port " << m_server->serverPort();

    return true;
}

void TcpTransport::stop()
{
    if (m_server) {
        delete m_connection.data();

        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

void TcpTransport::onRequest(const RequestHandler& onRequest)
{
    m_onRequest = onRequest;
}
