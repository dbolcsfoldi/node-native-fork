/**
MIT License

Copyright (c) 2018 David Bolcsfoldi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "header-ipc.h"

#include <iostream>

static const std::string kPing = "ping";

int main(int argc, char *argv[]) {
  hipc::IPC ipc;

  ipc.message.On([&](struct json_object *message) {
    std::cout << "Received message:" << std::endl;
    std::cout << json_object_to_json_string_ext(message, JSON_C_TO_STRING_SPACED) << std::endl;
    struct json_object *cmd = nullptr;
    
    if (json_object_object_get_ex(message, "cmd", &cmd) &&
        json_object_get_string(cmd) == kPing) {
      struct json_object *reply = json_object_new_object();

      json_object_object_add(reply, "cmd", json_object_new_string("pong"));
      ipc.Send(reply);
      json_object_put(reply);  
    } else {
      ipc.Send("{ \"error\": \"Unknown command\" }");
    }
  });

  ipc.disconnect.On([]() {
    std::cout << "Disconnected." << std::endl;
  });

  ipc.error.On([](const hipc::Error &error) {
    std::cout << "Error: " << error.error() << std::endl; 
  });
  
  return ipc.Run();
}

