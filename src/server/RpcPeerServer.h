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
#ifndef RPC_PEER_SERVER_H_
#define RPC_PEER_SERVER_H_

#include <json/json.h>
#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <mutex>

#include "../interface/IPeerServerConnector.h"
#include "../interface/IPeerServerHandler.h"
#include "../interface/IRpcEventManagerListener.h"
#include "../Log.h"
#include "RpcEventManager.h"

namespace rpcserver {

/***************************************************************
 *
 * *************************************************************/
template<typename H, typename C>
class RpcPeerServer : public IPeerServerHandler<H>
{
private:
  static constexpr auto RPC_METHOD_PARAM_EVENT = "event";
  static constexpr auto RPC_METHOD_PARAM_ID = "id";
  static constexpr auto RPC_METHOD_PARAM_LISTENERS = "listeners";

public:
  /***************************************************************
   *
   * *************************************************************/
  RpcPeerServer(AbstractPeerServerConnector<H>& connector,
                const std::string& registerMethodName,
                const std::string& unregisterMethodName,
                const std::string& getListenersMethodName,
                std::vector<std::pair<std::string, std::string>> providers)
    : m_connector(connector)
    , m_registerMethodName(registerMethodName)
    , m_unregisterMethodName(unregisterMethodName)
    , m_getListenersMethodName(getListenersMethodName)
  {
    if (registerMethodName.empty() || unregisterMethodName.empty() || getListenersMethodName.empty()) {
      throw std::invalid_argument("All 'register', 'unregister' and 'getListeners' methods should be provided");
    }

    m_connector.setPeerHandler(this);

    // Initialize event manager
    m_eventManager = std::unique_ptr<RpcEventManager<H, C>>(new RpcEventManager<H, C>());
    m_eventManager->setEventListener(std::make_shared<EventManagerListener<H>>(this));

    std::lock_guard<std::mutex> lk(m_mtx);
    for (const auto& provider : providers) {
      m_providersMap[provider.first] = {H(), provider.second};
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual ~RpcPeerServer() = default;

  /***************************************************************
   *
   * *************************************************************/
  virtual std::vector<std::string> getMethodList() override
  {
    std::vector<std::string> result;

    result.push_back(m_registerMethodName);
    result.push_back(m_unregisterMethodName);
    result.push_back(m_getListenersMethodName);

    {
      std::lock_guard<std::mutex> lk(m_mtx);
      for (const auto &provider: m_providersMap) {
        result.push_back(provider.second.second);
      }
    }

    for (auto& pair : m_methodTable) {
      result.push_back(pair.first);
    }
    return result;
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual std::vector<std::string> getNotificationsList() override
  {
    std::vector<std::string> result;
    for (auto& pair : m_notificationsTable) {
      result.push_back(pair.first);
    }
    return result;
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual bool bindMethod(const std::string& name,
                          std::function<void(const Json::Value&, Json::Value&)> method)
  {
    if (name.empty()) {
      throw std::invalid_argument("Method name should not be empty");
    }
    if (!symbolExists(name)) {
      m_methodTable[name] = method;
      return true;
    }
    return false;
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual bool bindNotification(const std::string& name,
                                std::function<void(const Json::Value&)> method)
  {
    if (name.empty()) {
      throw std::invalid_argument("Notification name should not be empty");
    }
    if (!symbolExists(name)) {
      m_notificationsTable[name] = method;
      return true;
    }
    return false;
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void HandleMethodCall(const std::string& name,
                                const H& connHdl,
                                const Json::Value& input,
                                Json::Value& output) override
  {
    if (name == m_registerMethodName) {
      HandleRegister(connHdl, input, output);
    } else if (name == m_unregisterMethodName) {
      HandleUnregister(connHdl, input, output);
    } else if (name == m_getListenersMethodName) {
      HandleGetListeners(connHdl, input, output);
    } else {
      {
        std::lock_guard<std::mutex> lk(m_mtx);
        for (const auto &entry: m_providersMap) {
          if (name == entry.second.second) {
            HandleProvide(name, entry.first, connHdl, input, output);
            return;
          }
        }
      }
      m_methodTable.at(name)(input, output);
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void HandleNotificationCall(const std::string& name,
                                      const H& clientCtx,
                                      const Json::Value& input) override
  {
    m_notificationsTable.at(name)(input);
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void onPeerConnected(const H& clientCtx) override {
    std::lock_guard<std::mutex> lk(m_mtx);
    const auto id = clientCtx.getClientId();
    if (!id.empty()) {
      auto& entry = m_providersMap[id]; // create if needed
      entry.first = clientCtx;
      rpcserver::log::warn("Registered client " + id + " on " + std::to_string(reinterpret_cast<uintptr_t>(&clientCtx)) + " via header (connected)");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void onPeerDisconnected(const H& clientCtx) override
  {
    m_eventManager->unregisterConn(clientCtx);

    std::lock_guard<std::mutex> lk(m_mtx);
    for(auto& entry: m_providersMap)
    {
      if (entry.second.first == clientCtx) {
        rpcserver::log::warn("Unregistering client " + entry.first + " on " + std::to_string(reinterpret_cast<uintptr_t>(&clientCtx)) + " (disconnected)");
        entry.second.first.clear();
      }
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void sendNotification(const H& clientCtx,
                                const std::string& name,
                                const Json::Value& params)
  {
    m_connector.sendNotification(clientCtx, name, params);
  }

  /***************************************************************
 *
 * *************************************************************/
  virtual void callMethod(const std::string& clientId, const std::string& name, const Json::Value& params, const RpcCbs& cbs)
  {
    try {
      std::unique_lock<std::mutex> lk(m_mtx);
      const auto& entry = m_providersMap.at(clientId);
      if (entry.first.valid()) {
        const auto clientCtx = entry.first;
        lk.unlock();
        m_connector.callMethod(clientCtx, name, params, cbs);
      } else {
        throw std::runtime_error("ClientId " + clientId + " has not been registered by any client");
      }
    } catch (const std::out_of_range& e) {
      throw std::runtime_error("ClientId " + clientId + " is unknown");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  void onEvent(const std::string& name, const Json::Value& eventInfo)
  {
    m_eventManager->onEvent(name, eventInfo);
  }

protected:
  /***************************************************************
   *
   * *************************************************************/
  template<typename X>
  class EventManagerListener : public IRpcEventManagerListener<X>
  {
  private:
    RpcPeerServer* m_parent;

  public:
    /***************************************************************
     *
     * *************************************************************/
    explicit EventManagerListener(RpcPeerServer* parent)
      : m_parent(parent)
    {}

    /***************************************************************
     *
     * *************************************************************/
    virtual ~EventManagerListener() {}

    /***************************************************************
     *
     * *************************************************************/
    virtual void onConnEvent(const X& clientCtx,
                             const std::string& name,
                             const Json::Value& eventData) override
    {
      m_parent->sendNotification(clientCtx, name, eventData);
    }
  };

  /***************************************************************
   *
   * *************************************************************/
  virtual void HandleRegister(const H& clientCtx, const Json::Value& input, Json::Value& output)
  {
    if (input.size() >= 2 && input.isMember(RPC_METHOD_PARAM_EVENT) &&
        input.isMember(RPC_METHOD_PARAM_ID) && input[RPC_METHOD_PARAM_EVENT].isString() &&
        input[RPC_METHOD_PARAM_ID].isString()) {
      std::string event = input[RPC_METHOD_PARAM_EVENT].asString();
      std::string id = input[RPC_METHOD_PARAM_ID].asString();

      int result = m_eventManager->registerToEvent(clientCtx, event, id);
      if (!result) {
        throw jsonrpc::JsonRpcException(-1,
                                        "Only one registration per event type per each connection");
      } else {
        output = 0;
      }
    } else {
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS,
                                      "Wrong parameters");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void HandleUnregister(const H& clientCtx, const Json::Value& input, Json::Value& output)
  {
    if (input.size() >= 2 && input.isMember(RPC_METHOD_PARAM_EVENT) &&
        input.isMember(RPC_METHOD_PARAM_ID) && input[RPC_METHOD_PARAM_EVENT].isString() &&
        input[RPC_METHOD_PARAM_ID].isString()) {
      std::string event = input[RPC_METHOD_PARAM_EVENT].asString();
      std::string id = input[RPC_METHOD_PARAM_ID].asString();

      bool result = m_eventManager->unregisterFromEvent(clientCtx, event, id);
      if (!result) {
        throw jsonrpc::JsonRpcException(-1, "Registration info not found");
      } else {
        output = 0;
      }
    } else {
      throw jsonrpc::JsonRpcException(jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS,
                                      "Wrong parameters");
    }
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void HandleGetListeners(const H& clientCtx, const Json::Value& input, Json::Value& output)
  {
    output[RPC_METHOD_PARAM_LISTENERS] = m_eventManager->getListenersOfEvents(clientCtx,
                                                                              RPC_METHOD_PARAM_EVENT,
                                                                              RPC_METHOD_PARAM_ID);
  }

  /***************************************************************
   *
   * *************************************************************/
  virtual void HandleProvide(const std::string& name, const std::string& clientId, const H& clientCtx, const Json::Value& input, Json::Value& output)
  {
    m_providersMap[clientId].first = clientCtx;
    output = Json::Value(0);
    std::cout << "Registered client " << clientId << " on " << clientCtx << " via provider function " << name << std::endl;
  }

  /***************************************************************
   *
   * *************************************************************/
  bool symbolExists(const std::string& name)
  {
    if (name == m_registerMethodName || name == m_unregisterMethodName || name == m_getListenersMethodName) {
      return true;
    }
    if (m_methodTable.find(name) != m_methodTable.end()) {
      return true;
    }
    if (m_notificationsTable.find(name) != m_notificationsTable.end()) {
      return true;
    }

    std::lock_guard<std::mutex> lk(m_mtx);
    for (const auto& entry : m_providersMap) {
      if (entry.second.second == name) {
        return true;
      }
    }
    return false;
  }

  AbstractPeerServerConnector<H>& m_connector;
  std::string m_registerMethodName;
  std::string m_unregisterMethodName;
  std::string m_getListenersMethodName;
  std::unique_ptr<RpcEventManager<H, C>> m_eventManager;
  std::map<std::string, std::function<void(const Json::Value&, Json::Value&)>> m_methodTable;
  std::map<std::string, std::function<void(const Json::Value&)>> m_notificationsTable;
  std::map<std::string, std::pair<H, std::string>> m_providersMap;
  std::mutex m_mtx;
};

} // namespace rpcserver

#endif // RPC_PEER_SERVER_H_
