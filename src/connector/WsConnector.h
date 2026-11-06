/*
	* If not stated otherwise in this file or this component's LICENSE file the
	* following copyright and licenses apply:
	*
	* Copyright 2021 Liberty Global Service BV
	*
	* Licensed under the Apache License, Version 2.0 (the "License");
	* you may not use this file except in compliance with the License.
	* You may obtain a copy of the License at
	*
	* http://www.apache.org/licenses/LICENSE-2.0
	*
	* Unless required by applicable law or agreed to in writing, software
	* distributed under the License is distributed on an "AS IS" BASIS,
	* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	* See the License for the specific language governing permissions and
	* limitations under the License.
	*/
#ifndef WS_CONNECTOR_H_
#define WS_CONNECTOR_H_

#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <iostream>
#include <mutex>

#include <boost/asio.hpp>
#include <json/json.h>
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/iclientconnector.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "AbstractPeerServerConnector.h"
#include "PeerProtocolHandler.h"
#include "WsClientCtx.h"
#include "../Log.h"

namespace rpcserver {

/***************************************************************
 *
 * *************************************************************/
class WsConnector : public AbstractPeerServerConnector<WsClientCtx>
{
  using Clock = std::chrono::steady_clock;
  struct MethodCallCbEntry
  {
    std::string name;
    Json::Value params;
    RpcCbs cbs;
    Clock::time_point timestamp{Clock::now()};

    MethodCallCbEntry() {}

    MethodCallCbEntry(const std::string& name_,
                      const Json::Value& params_,
                      const RpcCbs& callbacks_):
      name{name_},
      params{params_},
      cbs{callbacks_}
    {}

    bool expired() const
    {
      using namespace std::chrono;

      const auto now = Clock::now();
      const auto diff = duration_cast<std::chrono::seconds>(now - timestamp);
      return cbs.timeout < diff;
    }
  };
public:
  /***************************************************************
   *
   * *************************************************************/
  WsConnector(const std::string& host, uint16_t port, bool ipv4only, std::size_t numThreads = 1, const bool sock_reuse_addr = true)
    : m_host(host)
    , m_port(port)
    , m_ipv4only(ipv4only)
    , m_numThreads(numThreads)
    , m_sock_reuse_addr(sock_reuse_addr)
  {
    if (m_numThreads == 0) {
      throw std::invalid_argument("numThreads should be > 0");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual ~WsConnector()
  {
    m_expiredMonitorActive = false;
    if (m_expiredMonitor.joinable()) {
      m_expiredMonitor.join();
    }
    WsConnector::StopListening();
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual bool StartListening() override
  {
    m_thread = std::thread(&WsConnector::serverLoop, this);
    return true;
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual bool StopListening() override
  {
    if (m_thread.joinable()) {
      m_server.get_io_service().dispatch(std::bind(&WsConnector::stopServerLoop, this));
      m_thread.join();
    }

    return true;
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void sendNotification(const WsClientCtx& clientCtx,
                                const std::string& name,
                                const Json::Value& params) override
  {
    m_server.get_io_service().dispatch(
      std::bind(&WsConnector::doSendNotification, this, clientCtx, name, params));
  }

  /***************************************************************
 *
 * *************************************************************/
  virtual void callMethod(const WsClientCtx& clientCtx,
                                const std::string& name,
                                const Json::Value& params,
                                const RpcCbs& cbs) override
  {
    {
      // only kick start expired calls thread when callMethod is ever used
      std::lock_guard<std::mutex> lk(m_mtx);
      if (!m_expiredMonitor.joinable()) {
        m_expiredMonitor = std::thread(&WsConnector::monitorExpired, this);
      }
    }
    m_server.get_io_service().dispatch(
      std::bind(&WsConnector::doCallMethod, this, clientCtx, name, params, cbs));
  }

protected:
  using Webserver = websocketpp::server<websocketpp::config::asio>;

  /***************************************************************
   *
   * *************************************************************/
  class WsClient : public jsonrpc::IClientConnector
  {
  protected:
    WsConnector* m_parent;
    WsClientCtx m_clientCtx;

  public:
    /***************************************************************
     *
     * *************************************************************/
    WsClient(WsConnector* parent, const WsClientCtx& clientCtx)
      : m_parent(parent)
      , m_clientCtx(clientCtx)
    {}

    /***************************************************************
     *
     * *************************************************************/
    virtual ~WsClient() {}

    /***************************************************************
     *
     * *************************************************************/
    virtual void SendRPCMessage(const std::string& message, std::string& result)
#if JSONRPC_CPP_MAJOR_VERSION == 0
    throw(jsonrpc::JsonRpcException)
#endif
    {
      m_parent->sendMessage(m_clientCtx, message);
    }
  };

  /***************************************************************
   *
   * *************************************************************/
  class PeerClient
  {
    std::unique_ptr<WsClient> wsClient;
    std::unique_ptr<jsonrpc::Client> client;

  public:
    /***************************************************************
     *
     * *************************************************************/
    PeerClient(WsConnector* parent, const WsClientCtx& clientCtx)
      : wsClient(std::unique_ptr<WsClient>(new WsClient(parent, clientCtx)))
      , client(std::unique_ptr<jsonrpc::Client>(new jsonrpc::Client(*wsClient)))
    {}

    /***************************************************************
     *
     * *************************************************************/
    void CallNotification(const std::string& name, const Json::Value& params)
    {
      client->CallNotification(name, params);
    }
  };

  /***************************************************************
   *
   * *************************************************************/
  class ClientCtxManager
  {
  protected:
    uint64_t m_connIdCnt = 0;
    std::map<websocketpp::connection_hdl, WsClientCtx, std::owner_less<websocketpp::connection_hdl>>
      m_connToCtx;
    std::map<WsClientCtx, websocketpp::connection_hdl, WsClientCtxComparator> m_ctxToConn;

  public:
    /***************************************************************
     *
     * *************************************************************/
    WsClientCtx newConnection(const websocketpp::connection_hdl& connHdl, const std::string& clientId)
    {
      WsClientCtx ctx(++m_connIdCnt, clientId);
      m_connToCtx[connHdl] = ctx;
      m_ctxToConn[ctx] = connHdl;
      return ctx;
    }

    /***************************************************************
     *
     * *************************************************************/
    WsClientCtx removeConnection(const websocketpp::connection_hdl& connHdl)
    {
      WsClientCtx ctx = m_connToCtx.at(connHdl);
      m_ctxToConn.erase(ctx);
      m_connToCtx.erase(connHdl);
      return ctx;
    }

    /***************************************************************
     *
     * *************************************************************/
    const WsClientCtx& getClientCtx(const websocketpp::connection_hdl& connHdl)
    {
      return m_connToCtx.at(connHdl);
    }

    /***************************************************************
     *
     * *************************************************************/
    const websocketpp::connection_hdl& getConnHdl(const WsClientCtx& ctx)
    {
      return m_ctxToConn.at(ctx);
    }

    std::vector<websocketpp::connection_hdl> getAllHdl()
    {
      std::vector<websocketpp::connection_hdl> result;
      for (auto& pair : m_connToCtx) {
        result.push_back(pair.first);
      }
      return result;
    }
  };

  /***************************************************************
   *
   * *************************************************************/
  void doSendResponse(const WsClientCtx& ctx, const std::string& message)
  {
    sendMessage(ctx, message);
  }

  /***************************************************************
   *
   * *************************************************************/
  void doSendNotification(const WsClientCtx& clientCtx,
                          const std::string& name,
                          const Json::Value& params)
  {
    try {
      m_clientHandlers.at(clientCtx)->CallNotification(name, params);
    } catch (std::out_of_range& e) {
      // This means client is already disconnected (and some notifications may still be queued),
      // it is normal behavior
    }
  }

  /***************************************************************
 *
 * *************************************************************/
  void doCallMethod(const WsClientCtx& clientCtx,
                    const std::string& name,
                    const Json::Value& params,
                    const RpcCbs& cbs)
  {
    uint64_t id = 0;
    {
      std::lock_guard<std::mutex> lk(m_mtx);
      id = m_nextid++;
      m_cbMap[id] = MethodCallCbEntry(name, params, cbs);
    }
    Json::FastWriter writer;
    Json::Value req;
    req["jsonrpc"] = "2.0";
    req["id"] = Json::UInt64(id);
    req["method"] = name;
    req["params"] = params;
    const auto msg = writer.write(req);
    bool success = sendMessage(clientCtx, msg);
    if (!success) {
      {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_cbMap.erase(id);
      }
      if (cbs.errorCb) {
        cbs.errorCb(name, params, Error{Error::CouldNotSendRequest, "Could not send request"});
      }
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void doHandleConnect(const WsClientCtx& ctx)
  {
    try {
      m_serverHandlers.at(std::this_thread::get_id())->onPeerConnected(ctx);
    } catch (const std::out_of_range& e) {
      throw std::logic_error("Server handler was not prepared for worker thread");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void doHandleDisconnect(const WsClientCtx& ctx)
  {
    try {
      m_serverHandlers[std::this_thread::get_id()]->onPeerDisconnected(ctx);
    } catch (const std::out_of_range& e) {
      throw std::logic_error("Server handler was not prepared for worker thread");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void doHandleRequestOrResponse(const WsClientCtx& ctx, const std::string& msg)
  {
    try {
      Json::Value jsonobj;
      Json::Reader reader;

      bool parsingSuccessful = reader.parse(msg, jsonobj);

      // is it a response to a method call?
      if (parsingSuccessful && !jsonobj.isMember("method")
          && jsonobj.isMember("id") && jsonobj["id"].isUInt64()
          && (jsonobj.isMember("result") || jsonobj.isMember("error"))) {
        uint64_t id = jsonobj["id"].asUInt64();

        std::unique_lock<std::mutex> lk(m_mtx);
        if (m_cbMap.find(id) != m_cbMap.end()) {
          const auto entry = m_cbMap[id];
          m_cbMap.erase(id);
          lk.unlock();

          if (jsonobj.isMember("result") && entry.cbs.doneCb) {
            entry.cbs.doneCb(entry.name, entry.params, jsonobj["result"]);
          } else if (jsonobj.isMember("error") && entry.cbs.errorCb) {
            const auto& error = jsonobj["error"];
            int code = Error::Unknown;
            std::string errormsg = "Unknown error message";
            if (error.isMember("code")) {
              code = error["code"].asInt();
            }
            if (error.isMember("message")) {
              errormsg = error["message"].asString();
            }
            entry.cbs.errorCb(entry.name, entry.params, Error(code, errormsg));
          }
        } else {
          std::cout << "Ignoring answer for "<< id << " : " << msg << std::endl;
        }
      } else { // assume incoming method call or event
        std::string response;
        m_serverHandlers.at(std::this_thread::get_id())->HandlePeerRequest(ctx, msg, response);
        if (!response.empty()) { // events do not have a response
          m_server.get_io_service().dispatch(
              std::bind(&WsConnector::doSendResponse, this, ctx, response));
        }
      }
    } catch (const std::out_of_range& e) {
      throw std::logic_error("Server handler was not prepared for worker thread");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  bool sendMessage(const WsClientCtx& ctx, const std::string& message)
  {
    try {
      const websocketpp::connection_hdl& connHdl = m_clientCtxManager.getConnHdl(ctx);
      try {
        m_server.send(connHdl, message, websocketpp::frame::opcode::text);
        return true;
      } catch (const websocketpp::lib::error_code& e) {
      } catch (const std::exception& e) {
      } catch (...) {
        // Ignore all send errors
      }
    } catch (const std::out_of_range& e) {
      // This means client is already disconnected (and some messages may still be queued),
      // it is normal behavior
    }
    return false;
  }

  /***************************************************************
   *
   * *************************************************************/
  void onConnOpen(websocketpp::connection_hdl connHdl)
  {
    auto con = m_server.get_con_from_hdl(connHdl);
    const auto& client_id = con->get_request_header("X-Client-ID");
    WsClientCtx ctx = m_clientCtxManager.newConnection(connHdl, client_id);
    m_clientHandlers[ctx] = std::unique_ptr<PeerClient>(new PeerClient(this, ctx));

    m_workerIoService.dispatch(std::bind(&WsConnector::doHandleConnect, this, ctx));
  }

  /***************************************************************
   *
   * *************************************************************/
  void onConnClose(websocketpp::connection_hdl connHdl)
  {
    try {
      WsClientCtx ctx = m_clientCtxManager.removeConnection(connHdl);
      m_clientHandlers.erase(ctx);

      m_workerIoService.dispatch(std::bind(&WsConnector::doHandleDisconnect, this, ctx));
    } catch (std::out_of_range& e) {
      throw std::runtime_error("Connection was closed before it was opened");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void onConnMessage(websocketpp::connection_hdl connHdl, Webserver::message_ptr msg)
  {
    const std::string& request = msg->get_payload();

    try {
      m_workerIoService.dispatch(std::bind(
        &WsConnector::doHandleRequestOrResponse, this, m_clientCtxManager.getClientCtx(connHdl), request));
    } catch (std::out_of_range& e) {
      throw std::runtime_error("Message was received before the connection was opened");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void addWorkerThreads(std::vector<std::thread>& threadPool)
  {
    std::size_t (boost::asio::io_service::*runPtr)() = &boost::asio::io_service::run;

    for (std::size_t i = 0; i < m_numThreads; i++) {
      threadPool.push_back(std::thread(std::bind(runPtr, &m_workerIoService)));

      if (m_peerServerHandler) {
        m_serverHandlers[threadPool.back().get_id()] =
          std::unique_ptr<PeerProtocolHandler<WsClientCtx>>(
            new PeerProtocolHandler<WsClientCtx>(m_peerServerHandler));
      }
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void joinWorkerThreads(std::vector<std::thread>& threadPool)
  {
    for (std::thread& t : threadPool) {
      std::thread::id id = t.get_id();
      t.join();
      m_serverHandlers.erase(id);
    }
    threadPool.clear();
  }

  /***************************************************************
   *
   * *************************************************************/
  void closeConnections()
  {
    for (auto& hdl : m_clientCtxManager.getAllHdl()) {
      auto conn = m_server.get_con_from_hdl(hdl);
      if (conn) {
        try {
          conn->close(websocketpp::close::status::going_away, "Server shutdown");
        } catch (const websocketpp::lib::error_code& e) {
          // Ignore all errors when closing connections
        }
      }
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void stopServerLoop()
  {
    m_server.stop_listening();
    closeConnections();
    // When all connections will be closed the underlying io_service will stop automatically (and server loop will exit)
  }

  /***************************************************************
   *
   * *************************************************************/
  void serverLoop()
  {
    try {
#if defined(RPCSERVER_ENABLE_VERBOSE_LOGGING) && RPCSERVER_ENABLE_VERBOSE_LOGGING
      // Set logging to be pretty verbose (everything except message payloads)
      m_server.set_access_channels(websocketpp::log::alevel::all);
      m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
#else
      m_server.clear_access_channels(websocketpp::log::alevel::all);
      // Turn off websocketpp logging except for warnings and errors
      m_server.clear_error_channels(websocketpp::log::elevel::all);
      m_server.set_error_channels(websocketpp::log::elevel::warn |
                                  websocketpp::log::elevel::rerror |
                                  websocketpp::log::elevel::fatal);
#endif

      // Initialize ASIO
      m_server.init_asio();
      m_server.set_reuse_addr(m_sock_reuse_addr);
      m_server.set_listen_backlog(boost::asio::socket_base::max_connections);

      // Init worker thread pool
      boost::asio::io_service::work work(m_workerIoService);
      std::vector<std::thread> workerThreadPool;
      addWorkerThreads(workerThreadPool);

      // Register handlers
      using websocketpp::lib::bind;
      using websocketpp::lib::placeholders::_1;
      using websocketpp::lib::placeholders::_2;

      m_server.set_open_handler(bind(&WsConnector::onConnOpen, this, _1));
      m_server.set_close_handler(bind(&WsConnector::onConnClose, this, _1));
      m_server.set_message_handler(bind(&WsConnector::onConnMessage, this, _1, _2));

      if (m_host.empty()) {
        m_server.listen(m_ipv4only ? websocketpp::lib::asio::ip::tcp::v4()
                                   : websocketpp::lib::asio::ip::tcp::v6(),
                        m_port);
      } else {
        m_server.listen(m_host, std::to_string(m_port));
      }

      // Start the server accept loop
      m_server.start_accept();

      // Start the ASIO io_service run loop
      m_server.run();

      // Dispose worker thread pool
      m_workerIoService.stop();
      joinWorkerThreads(workerThreadPool);

    } catch (...) {
      throw; // Rethrow any exception
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void monitorExpired()
  {
    while(m_expiredMonitorActive) {
      std::this_thread::sleep_for(std::chrono::milliseconds{500});
      MethodCallCbEntry expired;
      {
        std::lock_guard<std::mutex> lk(m_mtx);
        auto begin = std::begin(m_cbMap);
        const auto end = std::end(m_cbMap);

        while (begin != end) {
          if (begin->second.expired()) {
            std::swap(expired, begin->second);
            begin = m_cbMap.erase(begin);
            break;
          } else std::advance(begin, 1);
        }
      }
      if (expired.cbs.errorCb) {
        expired.cbs.errorCb(expired.name, expired.params,
                            Error{Error::TimeoutWaitingOnResponse, "Timed-out waiting on response"});
      }
    }
  }

  Webserver m_server;
  std::thread m_thread;
  std::string m_host;
  uint16_t m_port;
  bool m_ipv4only;
  std::size_t m_numThreads;
  bool m_sock_reuse_addr;
  boost::asio::io_service m_workerIoService;
  std::map<std::thread::id, std::unique_ptr<PeerProtocolHandler<WsClientCtx>>> m_serverHandlers;
  std::map<WsClientCtx, std::unique_ptr<PeerClient>, WsClientCtxComparator> m_clientHandlers;
  ClientCtxManager m_clientCtxManager;
  std::map<uint64_t, MethodCallCbEntry> m_cbMap;
  std::mutex m_mtx;
  uint64_t m_nextid = 1;
  std::atomic<bool> m_expiredMonitorActive{true};
  std::thread m_expiredMonitor;
};

} // namespace rpcserver

#endif // WS_CONNECTOR_H_
