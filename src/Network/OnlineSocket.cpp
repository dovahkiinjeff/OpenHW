#include "OnlineClient.h"
#include "OnlineBuild.h"
#include <windows.h>
#include <winhttp.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
static_assert(sizeof(OHConfig)==660 && sizeof(OHSlot)==68 && sizeof(OHLoad)==668,"Protocol ABI changed");
namespace {
struct Message {unsigned type;std::vector<unsigned char> data;};
struct Connection {
    std::atomic<bool> stop{false},connected{false};
    std::atomic<HINTERNET> socket{nullptr},request{nullptr};
    std::thread reader,writer;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Message> incoming,outgoing;
    size_t inboundBytes=0,outboundBytes=0;
    std::string status="Offline";
    ~Connection(){close();}
    void setStatus(const char *text){std::lock_guard lock(mutex);status=text;}
    void close() {
        stop=true;connected=false;wake.notify_all();
        if(auto s=socket.exchange(nullptr))WinHttpCloseHandle(s);
        if(auto r=request.exchange(nullptr))WinHttpCloseHandle(r);
        if(reader.joinable())reader.join();if(writer.joinable())writer.join();
        std::lock_guard lock(mutex);incoming.clear();outgoing.clear();inboundBytes=outboundBytes=0;
    }
    void transmit() {
        while(!stop) {
            Message msg;
            {std::unique_lock lock(mutex);wake.wait(lock,[&]{return stop||!outgoing.empty();});
                if(stop)return;msg=std::move(outgoing.front());outgoing.pop_front();outboundBytes-=msg.data.size();}
            OHHeader header{{'O','H','M','P'},OH_PROTOCOL,(uint16_t)msg.type,(uint32_t)msg.data.size()};
            std::vector<unsigned char> bytes(sizeof(header)+msg.data.size());
            std::memcpy(bytes.data(),&header,sizeof(header));if(!msg.data.empty())std::memcpy(bytes.data()+sizeof(header),msg.data.data(),msg.data.size());
            auto s=socket.load();
            if(!s || WinHttpWebSocketSend(s,WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,bytes.data(),(DWORD)bytes.size())!=NO_ERROR) {
                stop=true;connected=false;setStatus("Connection lost");
                if(auto r=request.exchange(nullptr))WinHttpCloseHandle(r);
                if(auto h=socket.exchange(nullptr))WinHttpCloseHandle(h);
                return;
            }
        }
    }
    void receive(OHHello hello) {
        HINTERNET session=nullptr,host=nullptr;
        std::wstring endpoint=L"https://openhw.tech/multiplayer/v1";
        wchar_t overrideUrl[2048];DWORD n=GetEnvironmentVariableW(L"OPENHW_MULTIPLAYER_URL",overrideUrl,2048);
        if(n && n<2048)endpoint=overrideUrl;
        wchar_t hostname[256]{},path[1024]{};URL_COMPONENTS url{};url.dwStructSize=sizeof(url);
        url.lpszHostName=hostname;url.dwHostNameLength=256;url.lpszUrlPath=path;url.dwUrlPathLength=1024;
        if(!WinHttpCrackUrl(endpoint.c_str(),0,0,&url)) {setStatus("Invalid server address");stop=true;return;}
        bool secure=url.nScheme==INTERNET_SCHEME_HTTPS;
        if(!secure && std::wcscmp(hostname,L"127.0.0.1") && std::wcscmp(hostname,L"localhost")) {
            setStatus("Online play requires HTTPS");stop=true;return;
        }
        session=WinHttpOpen(L"OpenHW dedicated multiplayer/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
        if(session)WinHttpSetTimeouts(session,10000,10000,5000,30000);
        if(session)host=WinHttpConnect(session,hostname,url.nPort,0);
        if(host && !stop)request=WinHttpOpenRequest(host,L"GET",path,nullptr,WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,secure?WINHTTP_FLAG_SECURE:0);
        auto r=request.load();
        if(!r || stop || !WinHttpSetOption(r,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,nullptr,0) ||
            !WinHttpSendRequest(r,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||
            !WinHttpReceiveResponse(r,nullptr)) {setStatus("Cannot reach the multiplayer server");goto done;}
        socket=WinHttpWebSocketCompleteUpgrade(r,0);
        if(auto h=request.exchange(nullptr))WinHttpCloseHandle(h);
        if(!socket || stop) {setStatus("Server connection was not accepted");goto done;}
        connected=true;setStatus("Connected / create or join a room");
        {std::lock_guard lock(mutex);outgoing.push_back({OH_HELLO,std::vector<unsigned char>((unsigned char*)&hello,(unsigned char*)&hello+sizeof(hello))});outboundBytes+=sizeof(hello);}
        writer=std::thread([this]{transmit();});wake.notify_one();
        {
            std::vector<unsigned char> bytes;bytes.reserve(OH_MAX_MESSAGE+sizeof(OHHeader));
            while(!stop) {
                unsigned char buffer[8192];DWORD size=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE kind;
                auto s=socket.load();if(!s)break;
                auto error=WinHttpWebSocketReceive(s,buffer,sizeof(buffer),&size,&kind);
                if(error || kind==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)break;
                if(kind!=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE && kind!=WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE)break;
                if(bytes.size()+size>OH_MAX_MESSAGE+sizeof(OHHeader))break;
                bytes.insert(bytes.end(),buffer,buffer+size);
                if(kind==WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
                    OHHeader header{};if(bytes.size()<sizeof(header))break;std::memcpy(&header,bytes.data(),sizeof(header));
                    if(std::memcmp(header.magic,"OHMP",4)||header.version!=OH_PROTOCOL||header.length>OH_MAX_MESSAGE||bytes.size()!=sizeof(header)+header.length)break;
                    {std::lock_guard lock(mutex);if(incoming.size()>=256 || inboundBytes+header.length>2*1024*1024)break;
                        incoming.push_back({header.type,std::vector<unsigned char>(bytes.begin()+sizeof(header),bytes.end())});inboundBytes+=header.length;}
                    bytes.clear();
                }
            }
        }
    done:
        connected=false;stop=true;wake.notify_all();
        if(auto s=socket.exchange(nullptr))WinHttpCloseHandle(s);
        if(auto h=request.exchange(nullptr))WinHttpCloseHandle(h);
        if(writer.joinable())writer.join();
        if(host)WinHttpCloseHandle(host);if(session)WinHttpCloseHandle(session);
        {std::lock_guard lock(mutex);if(status.rfind("Connected",0)==0)status="Disconnected from server";}
    }
};
Connection connection;
}
extern "C" void hwOnlineFingerprint(unsigned char output[32]) {
    const char *hex=OH_BUILD_FINGERPRINT;
    for(unsigned i=0;i<32;i++){unsigned v=0;std::sscanf(hex+i*2,"%2x",&v);output[i]=(unsigned char)v;}
}
extern "C" void hwOnlineConnect(const char *name) {
    connection.close();connection.stop=false;connection.setStatus("Connecting to OpenHW server...");
    OHHello hello{};std::snprintf(hello.name,sizeof(hello.name),"%.17s",name&&*name?name:"Player");hwOnlineFingerprint(hello.fingerprint);
    connection.reader=std::thread([hello]{connection.receive(hello);});
}
extern "C" void hwOnlineDisconnect(void){connection.close();connection.setStatus("Offline");}
extern "C" int hwOnlineConnected(void){return connection.connected;}
extern "C" int hwOnlineSend(unsigned type,const void *data,size_t length) {
    if(!connection.connected || connection.stop || length>OH_MAX_MESSAGE || (length&&!data))return 0;
    {std::lock_guard lock(connection.mutex);if(connection.outgoing.size()>=256 || connection.outboundBytes+length>2*1024*1024)return 0;
        Message message;message.type=type;if(length)message.data.assign((const unsigned char*)data,(const unsigned char*)data+length);
        connection.outgoing.push_back(std::move(message));connection.outboundBytes+=length;}
    connection.wake.notify_one();return 1;
}
extern "C" int hwOnlineReceive(unsigned *type,void *data,size_t *length) {
    std::lock_guard lock(connection.mutex);
    if(connection.incoming.empty())return 0;
    auto& msg=connection.incoming.front();if(*length<msg.data.size())return 0;
    *type=msg.type;*length=msg.data.size();if(*length)std::memcpy(data,msg.data.data(),*length);
    connection.inboundBytes-=*length;connection.incoming.pop_front();return 1;
}
extern "C" void hwOnlineStatus(char *text,size_t capacity) {
    std::lock_guard lock(connection.mutex);std::snprintf(text,capacity,"%s",connection.status.c_str());
}
extern "C" int hwOnlineProbeRequested(int argc,char **argv) {
    for(int i=1;i<argc;i++)if(!std::strcmp(argv[i],"--online-probe"))return 1;
    return 0;
}
/* Exercise the actual shipped TLS/socket implementation without a window. */
extern "C" int hwOnlineProbeMain(void) {
    bool created=false,leaving=false;unsigned type;unsigned char data[OH_MAX_MESSAGE];
    auto deadline=GetTickCount64()+20000;
    hwOnlineConnect("Transport probe");
    while(GetTickCount64()<deadline) {
        size_t size=sizeof(data);
        if(hwOnlineReceive(&type,data,&size)) {
            if(type==OH_ERROR) {std::fprintf(stderr,"ONLINE probe rejected: %.*s\n",(int)size,data);break;}
            if(type==OH_WELCOME) {
                if(leaving){hwOnlineDisconnect();std::fprintf(stderr,"ONLINE TLS hello, room creation and leave passed\n");return 0;}
                if(!created){hwOnlineSend(OH_CREATE,nullptr,0);created=true;}
            } else if(type==OH_LOBBY && size==sizeof(OHLobby)) {
                auto *room=(const OHLobby*)data;
                if(!room->host || room->config.slots[room->ownSlot].kind!=1)break;
                hwOnlineSend(OH_LEAVE,nullptr,0);leaving=true;
            }
        }
        Sleep(10);
    }
    char status[256];hwOnlineStatus(status,sizeof(status));
    std::fprintf(stderr,"ONLINE probe failed: %s\n",status);hwOnlineDisconnect();return 1;
}
