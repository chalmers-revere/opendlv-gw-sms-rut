/*
 * Copyright (C) 2019 Ola Benderius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <string>

#include <curl/curl.h>

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

int32_t main(int32_t argc, char **argv)
{
  int32_t retCode{1};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ( (0 == commandlineArguments.count("url")) ||
      (0 == commandlineArguments.count("user")) ||
      (0 == commandlineArguments.count("password")) ||
      (0 == commandlineArguments.count("cid")) ) {
    std::cerr << argv[0] << " sends and receives SMS text messages through a "
      << "Teltonika RUT 4G router." << std::endl
      << "Usage:   " << argv[0] << " --url=<URL> --cid=<CID> --user=<Username> "
      << "--password=<Password> [--id=<ID>] [--verbose]" << std::endl
      << "Example: " << argv[0] << " --url=http://192.168.0.1:8080 "
      << "--verbose --cid=111" << std::endl;
  } else {
    std::string const url{commandlineArguments["url"]};
    std::string const user{commandlineArguments["user"]};
    std::string const password{commandlineArguments["password"]};
    uint32_t senderStamp{(commandlineArguments.count("id") != 0) ?
        static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};
    bool verbose{commandlineArguments.count("verbose") != 0};

    cluon::OD4Session od4{
      static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

    CURL *curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);

    auto onRemoteMessageRequest{[&curl, &url, &user, &password, &senderStamp,
      &verbose](cluon::data::Envelope &&envelope)
      {
        if (envelope.senderStamp() == senderStamp) {
          auto msg = cluon::extractMessage<opendlv::proxy::RemoteMessageRequest>(
              std::move(envelope));

          std::string address = msg.address();
          std::string message = msg.message();
          
          char *messageEncoded = curl_easy_escape(curl, message.c_str(), 
              message.length());
          std::string cmd = url + "/cgi-bin/sms_send?username=" + user 
            + "&password=" + password + "&number=" + address + "&text=" 
            + std::string(messageEncoded);
          curl_free(messageEncoded);

          curl_easy_setopt(curl, CURLOPT_URL, cmd.c_str());
          curl_easy_perform(curl);

          if (verbose) {
            std::cout << "Sending remote message '" << message << "' to " 
              << address << std::endl;
          }
        }
      }};

    od4.dataTrigger(opendlv::proxy::RemoteMessageRequest::ID(),
        onRemoteMessageRequest);

    while (od4.isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    curl_easy_cleanup(curl);

    retCode = 0;
  }
  return retCode;
}

