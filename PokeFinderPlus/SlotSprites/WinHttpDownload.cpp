#include "WinHttpDownload.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <QElapsedTimer>
#include <QSaveFile>
#include <QUrl>
#include <stdexcept>

namespace
{
struct InternetHandle
{
    HINTERNET value;
    explicit InternetHandle(HINTERNET handle) : value(handle) {}
    ~InternetHandle() { if(value) WinHttpCloseHandle(value); }
    InternetHandle(const InternetHandle &) = delete;
};
void checked(bool ok, const char *operation)
{
    if(!ok) throw std::runtime_error(QString("%1 (Windows error %2).")
        .arg(operation).arg(GetLastError()).toStdString());
}
}
namespace SlotSprites
{
// Synchronous WinHTTP calls run exclusively on the installer's worker thread.
// No application/system proxy, TLS, certificate, or Qt runtime settings are changed.
void downloadArchive(const QString &address, const QString &destination, std::atomic_bool &cancel,
                     std::atomic_int &progress)
{
    QElapsedTimer elapsed; elapsed.start();
    auto active = [&] {
        if(cancel) throw std::runtime_error("Sprite setup canceled.");
        if(elapsed.elapsed()>180000) throw std::runtime_error("Sprite download timed out.");
    };
    active();
    const QUrl url(address);
    if(!url.isValid() || url.scheme()!="https" || url.host().isEmpty())
        throw std::runtime_error("Invalid HTTPS sprite archive URL.");
    InternetHandle session(WinHttpOpen(L"PokeFinderPlus-SlotSprites/2", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    checked(session.value!=nullptr,"Could not initialize WinHTTP");
    checked(WinHttpSetTimeouts(session.value,10000,10000,10000,10000),"Could not set network timeouts");
    DWORD retries=1;
    checked(WinHttpSetOption(session.value,WINHTTP_OPTION_CONNECT_RETRIES,&retries,sizeof(retries)),"Could not limit connection retries");
    const auto host=url.host().toStdWString();
    InternetHandle connection(WinHttpConnect(session.value,host.c_str(),static_cast<INTERNET_PORT>(url.port(443)),0));
    checked(connection.value!=nullptr,"Could not connect to GitHub"); active();
    QString path=url.path(QUrl::FullyEncoded);
    if(url.hasQuery()) path+='?'+url.query(QUrl::FullyEncoded);
    const auto object=path.toStdWString();
    InternetHandle request(WinHttpOpenRequest(connection.value,L"GET",object.c_str(),nullptr,
                                             WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    checked(request.value!=nullptr,"Could not create HTTPS request");
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP, redirects=5;
    checked(WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy)),"Could not secure redirects");
    checked(WinHttpSetOption(request.value,WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS,&redirects,sizeof(redirects)),"Could not limit redirects");
    checked(WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0),"Could not send HTTPS request");
    active();
    checked(WinHttpReceiveResponse(request.value,nullptr),"Could not receive GitHub response"); active();
    DWORD status=0,size=sizeof(status);
    checked(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX),"Could not read HTTP status");
    if(status!=200) throw std::runtime_error(QString("GitHub returned HTTP %1.").arg(status).toStdString());
    DWORD total=0; size=sizeof(total);
    WinHttpQueryHeaders(request.value,WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,&total,&size,WINHTTP_NO_HEADER_INDEX);
    constexpr qint64 limit=32*1024*1024;
    if(total>limit) throw std::runtime_error("Sprite archive exceeds the download limit.");
    QSaveFile file(destination);
    if(!file.open(QIODevice::WriteOnly)) throw std::runtime_error("Could not create temporary download file.");
    qint64 received=0;
    char buffer[65536];
    for(;;)
    {
        active(); DWORD bytes=0;
        checked(WinHttpReadData(request.value,buffer,sizeof(buffer),&bytes),"Could not read sprite download");
        active(); if(!bytes) break;
        received+=bytes;
        if(received>limit || file.write(buffer,bytes)!=bytes) throw std::runtime_error("Could not save sprite download within its size limit.");
        progress=total ? static_cast<int>(qMin<qint64>(99,received*100/total)) : -1;
    }
    if(!received || (total && received!=total)) throw std::runtime_error("Sprite download was incomplete.");
    active();
    if(!file.commit()) throw std::runtime_error("Could not finish saving sprite download.");
    progress=100;
}
}
