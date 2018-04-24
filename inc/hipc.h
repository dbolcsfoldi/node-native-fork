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

#ifndef __HEADER_IPC_H__
#define __HEADER_IPC_H__

#include <functional>
#include <vector>
#include <string>
#include <sstream>
#include <cassert>

#include <uv.h>
#include <json-c/json.h>

namespace hipc {
  template <typename ...Args> class Event;

  template <typename ...Args> class Listener {
    public:
      void Remove() {
        owner_.RemoveListener(*this);
      }

      bool operator ==(const Listener<Args...> &to) const {
        return (id_ == to.id_);
      }

      bool operator !=(const Listener<Args...> &to) const {
        return (id_ != to.id_);
      }

      Listener(const Listener<Args...> &from) : 
        owner_(from.owner_),
        id_(from.id_),
        fun_(from.fun_) {}

      Listener(Event<Args...> &owner, uint64_t id, std::function<void(Args...)> fun) :
        owner_(owner),
        id_(id),
        fun_(fun) {}

    private:
      Event<Args...> owner_;
      uint64_t id_;
      std::function<void(Args...)> fun_;
      friend class Event<Args...>;
  };

  template <typename ...Args> class Event {
    public:
      Event() : id_(0) {}
      ~Event() {}

      Listener<Args...> On(const std::function<void(Args...)> &handler) {
        Listener<Args...> listener(*this, ++id_, handler);
        on_.push_back(listener);
        return listener;
      }

      Listener<Args...> Once(const std::function<void(Args...)> &handler) {
        Listener<Args...> listener(*this, ++id_, handler);
        once_.push_back(listener);
        return listener;
      }
      
      void RemoveListener(const Listener<Args...> &listener) {
        auto onceFound = std::find(once_.begin(), once_.end(), listener);
        if (onceFound != once_.end()) { once_.erase(onceFound); }
        
        auto onFound = std::find(on_.begin(), on_.end(), listener);
        if (onFound != on_.end()) { on_.erase(onFound); }
      }

      void RemoveAllListeners(void) {
        once_.clear();
        on_.clear();
      }
      
      void Emit(Args... args) {
        for (auto listener : once_) { listener.fun_(args...); }
        once_.clear();
        for (auto listener : on_) { listener.fun_(args...); }
      }

      std::vector<Listener<Args...>> Listeners() const {
        std::vector<Listener<Args...>> listeners = on_;
        listeners.insert(listeners.end(), once_.begin(), once_.end());

        return listeners;
      }

    private:
      std::vector<Listener<Args...>> on_;
      std::vector<Listener<Args...>> once_;
      mutable uint64_t id_;
  };

  class Error {
    public:
      Error() : error_(0) {}
      Error(int err) : error_(err) {}
    
      void setError(int err) {
        error_ = err;
      }

      int error() const {
        return error_;
      }
      
      operator bool () const {
        return (error_ == 0);
      }

    private:
      int error_;
  };

  struct WriteRequest {
    uv_buf_t buf;
    uv_write_t req;

    WriteRequest(const void *base, size_t len) {
      void *data = malloc(len);
      memcpy(data, base, len);

      buf.base = static_cast<char *>(data);
      buf.len = len;
    }

    WriteRequest(const WriteRequest &from) = delete;
    WriteRequest(WriteRequest &&from) = default;

    ~WriteRequest() {
      free(buf.base);
    }

    WriteRequest & operator =(const WriteRequest &from) = default;
  };

  static bool to_int(const char *str, int *i) {
    if (!str) {
     return false;
    } 
    
    char *end = nullptr;
    *i = strtoul(str, &end, 10);
    return (errno == EINVAL);
  }

  static const std::string kInternalPrefix = "NODE_";
  static const std::string kNodeChannelFd = "NODE_CHANNEL_FD"; 

  class IPC {
    public:
      IPC() : node_fd_(-1), tokener_(json_tokener_new()) {
        if (to_int(getenv(kNodeChannelFd.c_str()), &node_fd_)) {
          node_fd_ = -1;
        }
      }

      ~IPC() {
        if (node_fd_ >= 0 && 
            !uv_is_closing(reinterpret_cast<uv_handle_t *>(&handle_))) {
          uv_read_stop(reinterpret_cast<uv_stream_t *>(&handle_));
          uv_close(reinterpret_cast<uv_handle_t *>(&handle_), IPC::onClose);
        }

        uv_loop_close(uv_default_loop());
        json_tokener_free(tokener_);
      }

      void Send(struct json_object *json) {
        const char *str = json_object_to_json_string_ext(json, JSON_C_TO_STRING_PLAIN);
        
        pendingRequests_.emplace_back(reinterpret_cast<const void *>(str), strlen(str));
        sendImpl(pendingRequests_.back());
      }

      void Send(const std::string &json) {
        pendingRequests_.emplace_back(reinterpret_cast<const void *>(json.c_str()), json.length());
        sendImpl(pendingRequests_.back());
      }

      int Run() {
        if (node_fd_ < 0) {
          return UV_EBADF; 
        }

        uv_pipe_init(uv_default_loop(), &handle_, true);
        handle_.data = this;

        uv_pipe_open(&handle_, node_fd_);
        uv_read_start(reinterpret_cast<uv_stream_t *>(&handle_), IPC::onAlloc, IPC::onRead);
        
        return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
      }

    private:
      static void onAlloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t* buf) {
        buf->base = reinterpret_cast<char *>(malloc(suggested_size));
        buf->len = suggested_size;
      }

      void handleMessage(struct json_object *json) {
        struct json_object *cmd = nullptr;
        
        if (json_object_object_get_ex(json, "cmd", &cmd) && 
            json_object_is_type(cmd, json_type_string)) {
          std::string cmdStr(json_object_get_string(cmd));

          if (cmdStr.substr(0, kInternalPrefix.length()) == kInternalPrefix) {
            assert(false);
            // TODO: Internal messages not handled yet
            return;
          }
        }

        message.Emit(json);
      }

      void onReadImpl(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
        if (nread < 0) {
          uv_read_stop(stream);
          uv_close(reinterpret_cast<uv_handle_t *>(stream), IPC::onClose);

          if (nread != UV_EOF) {
            error.Emit(Error(nread));
          }

          return;
        }

        str_.write(buf->base, nread);
        
        for (std::string line; std::getline(str_, line); ) {
          json_tokener_reset(tokener_);
          struct json_object *json = json_tokener_parse(line.c_str());

          if (!json) {
            continue;
          }

          struct json_object *nodeHandle = nullptr;
          if (json_object_object_get_ex(json, "NODE_HANDLE", &nodeHandle) && 
              uv_pipe_pending_count(&handle_) > 0) {
            assert(false);
            // TODO: Implement passing file handles
          }
 
          handleMessage(json);
          json_object_put(json);
        }

      }

      static void onRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
        IPC *ipc = static_cast<IPC *>(stream->data);
        ipc->onReadImpl(stream, nread, buf);  
      }

      static void onClose(uv_handle_t *handle) {
        IPC *ipc = static_cast<IPC *>(handle->data);
        ipc->disconnect.Emit();
      }

      void onWriteImpl(uv_write_t *req, int status) {
        auto found = std::find_if(pendingRequests_.begin(), pendingRequests_.end(), [req] (const WriteRequest &r) -> bool {
            return (&r.req == req);
        });

        assert(found != pendingRequests_.end());
        pendingRequests_.erase(found);
        
        Error err(status);
        if (!err) {
          error.Emit(err);
        }
      }

      static void onWrite(uv_write_t *req, int status) {
        IPC *ipc = reinterpret_cast<IPC *>(req->handle->data);
        ipc->onWriteImpl(req, status);
      }

      void sendImpl(WriteRequest &req) {
        static char new_line[1] = { '\n' };

        uv_buf_t bufs[2] = {
          { .base = req.buf.base, .len = req.buf.len },
          { .base = new_line, .len = 1 }
        };
    
        Error err(uv_write(&req.req, 
              reinterpret_cast<uv_stream_t *>(&handle_), 
              bufs,
              2,
              IPC::onWrite));

        if (!err) {
          error.Emit(err);
        }
      }

    public:
      Event<> disconnect;
      Event<struct json_object *> message;
      Event<Error> error;

    private:
      int node_fd_;
      struct json_tokener *tokener_;

      uv_pipe_t handle_;
      std::stringstream str_;
      std::vector<WriteRequest> pendingRequests_;
  }; 
}

#endif // __HEADER_IPC_H__


