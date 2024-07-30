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
#include "WsRpcServerBuilderExtended.h"

using namespace rpcserver;

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilderExtended::~WsRpcServerBuilderExtended() = default;

/***************************************************************
 *
 * *************************************************************/
WsRpcServerBuilderExtended& WsRpcServerBuilderExtended::enableProviders(std::initializer_list<std::pair<std::string, std::string>> providers)
{
  m_providers.assign(providers.begin(), providers.end());
  return *this;
}

/***************************************************************
 *
 * *************************************************************/
IAbstractRpcServer* WsRpcServerBuilderExtended::build() const
{
  return new WsRpcServer(
    m_host, m_port, m_ipv4only, m_registerMethodName, m_unregisterMethodName, m_getListenersMethodName, m_providers, m_numThreads, m_sock_reuse_addr);
}
