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
#ifndef WS_RPC_SERVER_H_
#define WS_RPC_SERVER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <json/json.h>

#include "IAbstractRpcServer.h"
#include "connector/WsClientCtx.h"
#include "connector/WsConnector.h"
#include "server/RpcPeerServer.h"

namespace rpcserver {

/***************************************************************
 *
 * *************************************************************/
class WsRpcServer : public IAbstractRpcServer
{
public:
  /***************************************************************
   *
   * *************************************************************/
  WsRpcServer(const std::string& interface,
              uint16_t port,
              bool ipv4only,
              const std::string& registerMethodName,
              const std::string& unregisterMethodName,
              const std::string& getListenersMethodName,
              std::vector<std::pair<std::string, std::string>> providers,
              std::size_t numThreads = 1,
              const bool sock_reuse_addr = true);

  /***************************************************************
   *
   * *************************************************************/
  virtual ~WsRpcServer();

  /***************************************************************
   *
   * *************************************************************/
  virtual bool StartListening() override;

  /***************************************************************
   *
   * *************************************************************/
  virtual bool StopListening() override;

  /***************************************************************
   *
   * *************************************************************/
  virtual bool bindMethod(const std::string& name,
                          std::function<void(const Json::Value&, Json::Value&)> method) override;

  /***************************************************************
   *
   * *************************************************************/
  virtual bool bindNotification(const std::string& name,
                                std::function<void(const Json::Value&)> method) override;

  /***************************************************************
   *
   * *************************************************************/
  virtual void onEvent(const std::string& name, const Json::Value& eventInfo) override;

  /***************************************************************
   *
   * *************************************************************/
  virtual void callMethod(const std::string& clientId,
                          const std::string& name,
                          const Json::Value& params,
                          const RpcCbs& cbs) override;

protected:
  std::unique_ptr<WsConnector> m_wsConnector;
  std::unique_ptr<RpcPeerServer<WsClientCtx, WsClientCtxComparator>> m_rpcServer;
};

} // namespace rpcserver

#endif // WS_RPC_SERVER_H_
