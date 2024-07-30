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
#ifndef WS_CLIENT_CTX_H_
#define WS_CLIENT_CTX_H_

#include <iostream>
#include <string>

namespace rpcserver {

/***************************************************************
 *
 * *************************************************************/
struct WsClientCtx {
  uint64_t m_id;
  bool m_valid;
  std::string m_clientId;
  WsClientCtx()
  {
    m_id = 0;
    m_valid = false;
  }

  /***************************************************************
   *
   * *************************************************************/
  explicit WsClientCtx(uint64_t id, const std::string& clientId)
    : m_id(id), m_valid(true), m_clientId(clientId)
  {}

  /***************************************************************
   *
   * *************************************************************/
   bool valid() const
   {
    return m_valid;
   }

  /***************************************************************
  *
  * *************************************************************/
  std::string getClientId() const
  {
    return m_clientId;
  }

  /***************************************************************
   *
   * *************************************************************/
  void clear()
  {
    m_valid = false;
    m_id = 0;
  }

  /***************************************************************
  *
  * *************************************************************/
  friend std::ostream& operator<<(std::ostream& os, const WsClientCtx& obj)
  {
    os << obj.m_id;
    return os;
  }

  /***************************************************************
  *
  * *************************************************************/
  bool operator==(const WsClientCtx& other) const {
    return this->m_id == other.m_id;
  }
};

/***************************************************************
 *
 * *************************************************************/
struct WsClientCtxComparator : public std::binary_function<WsClientCtx, WsClientCtx, bool>
{
  /***************************************************************
   *
   * *************************************************************/
  bool operator()(const WsClientCtx& lhs, const WsClientCtx& rhs) const
  {
    return lhs.m_id < rhs.m_id;
  }
};

} // namespace rpcserver

#endif // WS_CLIENT_CTX_H_
