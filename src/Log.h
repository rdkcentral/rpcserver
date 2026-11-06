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

#ifndef RPCSERVER_LOG_H_
#define RPCSERVER_LOG_H_

#include <iostream>
#include <string>

namespace rpcserver {
namespace log {

inline void info(const std::string& message)
{
#if defined(RPCSERVER_ENABLE_VERBOSE_LOGGING) && RPCSERVER_ENABLE_VERBOSE_LOGGING
    std::clog << "[info] " << message << std::endl;
#endif
}

inline void warn(const std::string& message)
{
    std::clog << "[warn] " << message << std::endl;
}

inline void error(const std::string& message)
{
    std::cerr << "[error] " << message << std::endl;
}

} // namespace log
} // namespace rpcserver

#endif // RPCSERVER_LOG_H_
