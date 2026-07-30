#ifndef _WSWRAP_IXWEBSOCKET_HPP
#define _WSWRAP_IXWEBSOCKET_HPP

#include <string>
#include <functional>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>

namespace wswrap {
    class WebSocket {
    public:
        WebSocket(const std::string& url,
            std::function<void()> onOpen = nullptr,
            std::function<void()> onClose = nullptr,
            std::function<void(const std::string&)> onMessage = nullptr,
            std::function<void(const std::string&)> onError = nullptr,
            const std::string& certStore = "")
        {
            ix::initNetSystem();

            // Set the URL
            client.setUrl(url);

            // CRITICAL FIX: Add the Archipelago subprotocol header using the correct API
            client.addSubProtocol("ap");

            // Set up event handlers using the correct IXWebSocket API
            client.setOnMessageCallback([this, onOpen, onClose, onMessage, onError](const ix::WebSocketMessagePtr& msg) {
                switch (msg->type) {
                case ix::WebSocketMessageType::Open:
                    if (onOpen) onOpen();
                    break;

                case ix::WebSocketMessageType::Close:
                    if (onClose) onClose();
                    break;

                case ix::WebSocketMessageType::Message:
                    if (onMessage) onMessage(msg->str);
                    break;

                case ix::WebSocketMessageType::Error:
                    if (onError) onError(msg->errorInfo.reason);
                    break;

                default:
                    break;
                }
                });

            // Start the WebSocket connection
            client.start();
        }

        ~WebSocket() {
            client.stop();
        }

        void send(const std::string& msg) {
            client.send(msg);
        }

        void poll() {
            // IXWebSocket handles polling internally in its thread
        }

        // APClient requires these to exist
        int get_ok_connect_interval() { return 0; }

    private:
        ix::WebSocket client;
    };

    using WS = WebSocket;
}

#endif // _WSWRAP_IXWEBSOCKET_HPP