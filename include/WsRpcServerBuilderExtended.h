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
#ifndef WS_RPC_SERVER_BUILDER_EXTENDED_H_
#define WS_RPC_SERVER_BUILDER_EXTENDED_H_

#include <cstdint>
#include <string>
#include <utility>
#include <initializer_list>
#include <vector>

#include "IRpcServerBuilder.h"
#include "WsRpcServerBuilder.h"

namespace rpcserver {

/***************************************************************
 *
 * *************************************************************/
class WsRpcServerBuilderExtended : public WsRpcServerBuilder
{
public:
  WsRpcServerBuilderExtended(uint16_t port, bool ipv4only = false) : WsRpcServerBuilder(port, ipv4only) {}

  WsRpcServerBuilderExtended(const std::string& host, uint16_t port): WsRpcServerBuilder(host, port) {}

  virtual ~WsRpcServerBuilderExtended();

  WsRpcServerBuilderExtended& enableProviders(std::initializer_list<std::pair<std::string, std::string>> providers);

  virtual IAbstractRpcServer* build() const override;

protected:
  std::vector<std::pair<std::string, std::string>> m_providers;
};

} // namespace rpcserver

#endif // WS_RPC_SERVER_BUILDER_EXTENDED_H_
