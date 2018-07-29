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

#include "hipc.h"

#include <iostream>

static const std::string kPing = "ping";
static const std::string kSocket = "socket";
static const std::string kPingJson = "{\"cmd\":\"ping\"}";

static void on_close(uv_handle_t *handle) {
  delete handle;
}

static void on_write(uv_write_t *req, int status) {
  assert(status > 0);
  uv_close(reinterpret_cast<uv_handle_t *>(req->handle), on_close);
}

int main(int argc, char *argv[]) {
  hipc::IPC ipc;

  ipc.message.On([&](struct json_object *message, uv_stream_t *sendHandle) {
    struct json_object *cmd = nullptr;
    
    if (!json_object_object_get_ex(message, "cmd", &cmd)) {
      ipc.Send("{ \"error\": \"Unknown message\" }");
      return;  
    }

    if (json_object_get_string(cmd) == kPing) {
      struct json_object *reply = json_object_new_object();

      json_object_object_add(reply, "cmd", json_object_new_string("pong"));
      ipc.Send(reply);
      json_object_put(reply);  
    } else if(json_object_get_string(cmd) == kSocket) {
      uv_write_t req;
      uv_buf_t buf[] = {
        { .base = const_cast<char *>(kPingJson.c_str()), .len = kPingJson.size() }
      };

      uv_write(&req, sendHandle, buf, 1, on_write);
    } else {
      ipc.Send("{ \"error\": \"Unsupported message\" }");
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

