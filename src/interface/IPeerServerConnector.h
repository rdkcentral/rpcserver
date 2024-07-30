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

#ifndef I_PEER_SERVER_CONNECTOR_H_
#define I_PEER_SERVER_CONNECTOR_H_

#include <string>

#include <json/json.h>

#include "IPeerServerHandler.h"
#include "../../include/IRpcMethodCaller.h"

namespace rpcserver {

/***************************************************************
 *
 * *************************************************************/
template<typename H>
class IPeerServerConnector
{
public:
  /***************************************************************
   *
   * *************************************************************/
  IPeerServerConnector() {}

  /***************************************************************
   *
   * *************************************************************/
  virtual ~IPeerServerConnector() = default;

  /***************************************************************
   *
   * *************************************************************/
  virtual bool StartListening() = 0;

  /***************************************************************
   *
   * *************************************************************/
  virtual bool StopListening() = 0;

  /***************************************************************
   *
   * *************************************************************/
  virtual void sendNotification(const H& clientCtx,
                                const std::string& name,
                                const Json::Value& params) = 0;

  /***************************************************************
   *
   * *************************************************************/
  virtual void callMethod(const H& clientCtx,
                          const std::string& name,
                          const Json::Value& params,
                          const RpcCbs& cbs) = 0;

  /***************************************************************
   *
   * *************************************************************/
  virtual void setPeerHandler(IPeerServerHandler<H>* peerServerHandler) = 0;
};

} // namespace rpcserver

#endif // I_PEER_SERVER_CONNECTOR_H_
