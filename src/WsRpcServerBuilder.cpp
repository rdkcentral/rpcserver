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
#include <string>
#include <stdexcept>

#include "IAbstractRpcServer.h"
#include "WsRpcServer.h"
#include "WsRpcServerBuilder.h"

using namespace rpcserver;

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilder::WsRpcServerBuilder(uint16_t port, bool ipv4only)
  : m_port(port)
  , m_ipv4only(ipv4only)
{}

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilder::WsRpcServerBuilder(const std::string& host, uint16_t port)
  : m_host(host)
  , m_port(port)
{}

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilder::~WsRpcServerBuilder() = default;

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilder& WsRpcServerBuilder::enableServerEvents(const std::string& registerMethodName,
                                                     const std::string& unregisterMethodName,
                                                     const std::string& getListenersMethodName)
{
  if (registerMethodName.empty() || unregisterMethodName.empty() || getListenersMethodName.empty()) {
    throw std::invalid_argument("All 'register', 'unregister' and 'getListeners' method names should not be empty");
  }
  m_registerMethodName = registerMethodName;
  m_unregisterMethodName = unregisterMethodName;
  m_getListenersMethodName = getListenersMethodName;
  return *this;
}

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilder& WsRpcServerBuilder::numThreads(std::size_t numThreads)
{
  m_numThreads = numThreads;
  return *this;
}

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilder& WsRpcServerBuilder::sock_reuse_addr(bool _sock_reuse_addr)
{
  m_sock_reuse_addr = _sock_reuse_addr;
  return *this;
}

/***************************************************************
 *
 * *************************************************************/
IAbstractRpcServer* WsRpcServerBuilder::build() const
{
  return new WsRpcServer(
    m_host, m_port, m_ipv4only, m_registerMethodName, m_unregisterMethodName, m_getListenersMethodName, std::vector<std::pair<std::string, std::string>>(), m_numThreads, m_sock_reuse_addr);
}
