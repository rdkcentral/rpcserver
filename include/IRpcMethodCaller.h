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
#ifndef I_RPC_METHOD_CALLER_H_
#define I_RPC_METHOD_CALLER_H_

#include <functional>
#include <string>
#include <chrono>
#include <json/json.h>

namespace rpcserver {

  struct Error
  {
    static constexpr int CouldNotSendRequest = -32001;
    static constexpr int Unknown = -32002;
    static constexpr int TimeoutWaitingOnResponse = -32003;

    int code{0};
    std::string message;

    Error(int c, std::string msg) {
      code = c;
      message = msg;
    }
  };

  using RpcDoneCb = std::function<void(const std::string& name, const Json::Value& params, const Json::Value& result)>;
  using RpcErrorCb = std::function<void(const std::string& name, const Json::Value& params, const Error& error)>;

  struct RpcCbs
  {
    RpcDoneCb doneCb{nullptr};
    RpcErrorCb errorCb{nullptr};
    std::chrono::seconds timeout = std::chrono::seconds{18};

    RpcCbs() {}
    RpcCbs(RpcDoneCb dnCb, RpcErrorCb errCb, std::chrono::seconds timeout_seconds = std::chrono::seconds{18}):
        doneCb{std::move(dnCb)},
        errorCb{std::move(errCb)},
        timeout(timeout_seconds)
    {}
  };

  /***************************************************************
   * Support for duplex JSON-RPC: methods calls from
   * server to client.
   * *************************************************************/
class IRpcMethodCaller
{
public:
  /***************************************************************
   *
   * *************************************************************/
  virtual ~IRpcMethodCaller() = default;

  /***************************************************************
   * Call a json-rpc method from server towards client:
   * target client identification supported in two ways
   * - either the client calls a pre-registered function call
   *   (using enableProviders on WsRpcServerBuilderExtended)
   * - either the client sets extra header X-Client-ID
   *   in connection request
   * Throws runtime_error when clientId not known or unregistered.
   * *************************************************************/
  virtual void callMethod(const std::string& clientId,
                          const std::string& name,
                          const Json::Value& params,
                          const RpcCbs& cbs) = 0;
};

} // namespace rpcserver

#endif // I_RPC_METHOD_CALLER_H_
