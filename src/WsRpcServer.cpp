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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <json/json.h>

#include "WsRpcServer.h"
#include "connector/WsClientCtx.h"
#include "connector/WsConnector.h"
#include "server/RpcPeerServer.h"

using namespace rpcserver;

/***************************************************************
 *
 * *************************************************************/
WsRpcServer::WsRpcServer(const std::string& host,
                         uint16_t port,
                         bool ipv4only,
                         const std::string& registerMethodName,
                         const std::string& unregisterMethodName,
                         const std::string& getListenersMethodName,
                         std::vector<std::pair<std::string, std::string>> providers,
                         std::size_t numThreads,
                         const bool sock_reuse_addr)
  : m_wsConnector(std::unique_ptr<WsConnector>(new WsConnector(host, port, ipv4only, numThreads, sock_reuse_addr)))
  , m_rpcServer(std::unique_ptr<RpcPeerServer<WsClientCtx, WsClientCtxComparator>>(
      new RpcPeerServer<WsClientCtx, WsClientCtxComparator>(*m_wsConnector,
                                                        registerMethodName,
                                                        unregisterMethodName,
                                                        getListenersMethodName,
                                                        providers)))
{}

/***************************************************************
 *
 * *************************************************************/
WsRpcServer::~WsRpcServer() = default;

/***************************************************************
 *
 * *************************************************************/
bool WsRpcServer::StartListening()
{
  return m_wsConnector->StartListening();
}

/***************************************************************
 *
 * *************************************************************/
bool WsRpcServer::StopListening()
{
  return m_wsConnector->StopListening();
}

/***************************************************************
 *
 * *************************************************************/
bool WsRpcServer::bindMethod(const std::string& name,
                             std::function<void(const Json::Value&, Json::Value&)> method)
{
  return m_rpcServer->bindMethod(name, method);
}

/***************************************************************
 *
 * *************************************************************/
bool WsRpcServer::bindNotification(const std::string& name,
                                   std::function<void(const Json::Value&)> method)
{
  return m_rpcServer->bindNotification(name, method);
}

/***************************************************************
 *
 * *************************************************************/
void WsRpcServer::onEvent(const std::string& name, const Json::Value& eventInfo)
{
  m_rpcServer->onEvent(name, eventInfo);
}

/***************************************************************
 *
 * *************************************************************/
void WsRpcServer::callMethod(const std::string& clientId, const std::string& name, const Json::Value& params, const RpcCbs& cbs)
{
  m_rpcServer->callMethod(clientId, name, params, cbs);
}