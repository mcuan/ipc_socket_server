#include <boost/asio.hpp>
#include <iostream>

#define REGISTRATION_ENDPOINT "server.socket"

enum ClientId : uint16_t {
    INVALID_ID = 0,
    CLIENT_A,
    CLIENT_B,
    CLIENT_C,
    LAST_ID
};


class SocketClient {
  public:
    SocketClient(uint16_t client_id, std::string client_name, std::string socket_addr) 
        : is_registered(false)
        , is_connected(false)
    {
        m_id = client_id;
        m_name = client_name;
        m_sockaddr = socket_addr;
    }

    uint16_t getClientId() const {
        return m_id;
    }

    std::string getClientName() const {
        return m_name;
    }

    std::string getSocketAddress() const {
        return m_sockaddr;
    }

    bool isRegistered() const {
        return is_registered;
    }

    void setRegistered(bool status) {
        is_registered = status;
    }

    bool isConnected() const {
        return is_connected;
    }

    void setConnected(bool status) {
        is_connected = status;
    }

  private:
    uint16_t m_id = ClientId::INVALID_ID;
    std::string m_name;
    std::string m_sockaddr;

    bool is_registered;
    bool is_connected;
};

static std::vector<SocketClient> ClientList = {
    SocketClient(ClientId::CLIENT_A, "CLIENT_A", "client_a.socket"),
    SocketClient(ClientId::CLIENT_B, "CLIENT_B", "client_b.socket"),
    SocketClient(ClientId::CLIENT_C, "CLIENT_C", "client_c.socket"),
};

class RegistrationManager {
  public:
    void init() {
        ::unlink(REGISTRATION_ENDPOINT);

        try {
            RegistrationListener m_listener(io_context);
            io_context.run();
        } 
        catch (std::exception& e) {
            printf("Exception: %s\n", e.what());
        }
    }

    class RegistrationListener {
      public:
        RegistrationListener(boost::asio::io_context& io_context)
            : m_endpoint(REGISTRATION_ENDPOINT)
            , m_acceptor(io_context, m_endpoint)
        {
            acceptClientAsync();
        }

      private:
        void acceptClientAsync() {
            printf("Accepting connections to server\n");
            m_acceptor.async_accept(
                [this](boost::system::error_code error, 
                    boost::asio::local::stream_protocol::socket socket)
                {
                    if (!error) {
                        printf("Received connection\n");
                        std::make_shared<RegistrationHandler>(std::move(socket))->handleRequest();
                    } else {
                        printf("Error: %s\n", error.message().c_str());
                    }

                    acceptClientAsync();
                });
        }

        boost::asio::local::stream_protocol::endpoint m_endpoint;
        boost::asio::local::stream_protocol::acceptor m_acceptor;
    };

    class RegistrationHandler
        : public std::enable_shared_from_this<RegistrationHandler> 
    {
      public:
        RegistrationHandler(boost::asio::local::stream_protocol::socket socket)
            : m_socket(std::move(socket))
            , client_id(ClientId::INVALID_ID)
        {
            printf("Handling new client registration request\n");
        }

        ~RegistrationHandler() { printf("Destroying registration session\n"); }

        void handleRequest() {
            auto self(shared_from_this());
            m_socket.async_read_some(boost::asio::buffer(&client_id, sizeof(uint16_t)),
                [this, self](boost::system::error_code error, std::size_t) {
                    if (!error) {
                        printf("Attempting to register client_id: %d\n", client_id);
                        if (registerClient(client_id)) {
                            auto this_client = std::find_if(ClientList.begin(), ClientList.end(),
                                [this](const SocketClient& client) {
                                    return (client.getClientId() == client_id);
                                });
                            sendClientAddress(this_client->getSocketAddress());
                        }
                    } else {
                        printf("Error: %s\n", error.message().c_str());
                    }
                });
        }

      private:
        bool registerClient(const uint16_t& client_id) {
            bool registration_status = false;

            auto this_client = std::find_if(ClientList.begin(), ClientList.end(),
                [client_id](const SocketClient& client) {
                    return (client.getClientId() == client_id);
                });

            if(this_client != ClientList.end()) {
                if(!this_client->isRegistered()) {
                    printf("Registering client_id: %d on socket address: %s\n",
                        this_client->getClientId(),
                        this_client->getSocketAddress().c_str());

                    this_client->setConnected(true);
                    registration_status = true;
                }
                else {
                    printf("Nope...this client_id[%d] is already registered!\n", client_id);
                }
            }
            else {
                printf("Nope...this client_id[%d] is already registered!\n", client_id);
            }

            return registration_status;
        }

        void sendClientAddress(const std::string& address) {
            printf("Sending socket address '%s' for client to connect\n", address.c_str());
            m_socket.write_some(boost::asio::buffer(address, address.size()));
            m_socket.close();
        }


        uint16_t client_id;
        boost::asio::local::stream_protocol::socket m_socket;
    };

  private:

    boost::asio::io_context io_context;
};

int main() {
    RegistrationManager RegMgr;

    RegMgr.init();

    return 0;
}