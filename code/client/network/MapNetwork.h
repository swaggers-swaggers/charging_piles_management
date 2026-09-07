#ifndef MAPNETWORK_H
#define MAPNETWORK_H

#include <QHostAddress>
#include <QNetworkProxyFactory>
#include <QTcpSocket>
#include <QUrl>

namespace MapNetwork {

// Only bypass a local proxy that explicitly refuses connections. A timeout or
// a remote proxy failure must not silently change the user's network policy.
inline QNetworkProxy checkedProxy(const QNetworkProxy &proxy)
{
    const QHostAddress address(proxy.hostName());
    if (proxy.type() != QNetworkProxy::HttpProxy
        && proxy.type() != QNetworkProxy::Socks5Proxy)
        return proxy;
    if (!address.isLoopback() && proxy.hostName() != QStringLiteral("localhost"))
        return proxy;

    QTcpSocket probe;
    probe.setProxy(QNetworkProxy::NoProxy);
    probe.connectToHost(proxy.hostName(), proxy.port());
    if (!probe.waitForConnected(300)
        && probe.error() == QAbstractSocket::ConnectionRefusedError)
        return QNetworkProxy(QNetworkProxy::NoProxy);
    return proxy;
}

inline void configure()
{
    const auto proxies = QNetworkProxyFactory::systemProxyForQuery(
        QNetworkProxyQuery(QUrl(QStringLiteral("https://tiles.openfreemap.org"))));
    // Explicit application proxy is shared by Qt Network and Qt WebEngine.
    QNetworkProxy::setApplicationProxy(checkedProxy(
        proxies.isEmpty() ? QNetworkProxy(QNetworkProxy::NoProxy) : proxies.first()));
}

} // namespace MapNetwork
#endif
