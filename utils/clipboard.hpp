#pragma once

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <poll.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <csignal>

namespace cb {
    class Clipboard {
    public:
        using ChangeCallback = std::function<void(const std::string &)>;

        Clipboard() = default;

        ~Clipboard() { stop(); }

        Clipboard(const Clipboard &) = delete;

        Clipboard &operator=(const Clipboard &) = delete;

        static bool bootstrap(int argc, char **argv) {
            if (argc > 0) self_argv0() = argv[0];
            if (argc >= 3 && std::strcmp(argv[1], "--wlclip-notify") == 0) {
                relay_stdin_to_fifo(argv[2]);
                return true;
            }
            return false;
        }

        bool copy(const std::string &text) {
            {
                std::lock_guard<std::mutex> lock(self_mutex_);
                last_self_text_ = text;
                skip_next_matching_ = true;
            }
            FILE *pipe = popen("wl-copy", "w");
            if (!pipe) return false;
            fwrite(text.data(), 1, text.size(), pipe);
            int rc = pclose(pipe);
            return rc == 0;
        }

        bool watch(ChangeCallback cb) {
            if (running_.exchange(true)) return true; // 已在监听

            std::string self_exe = resolve_self_exe();
            if (self_exe.empty()) {
                running_ = false;
                return false;
            }

            fifo_path_ = "/tmp/wlclip_" + std::to_string(getpid()) + ".fifo";
            unlink(fifo_path_.c_str());
            if (mkfifo(fifo_path_.c_str(), 0600) != 0) {
                running_ = false;
                return false;
            }

            pid_t pid = fork();
            if (pid < 0) {
                running_ = false;
                unlink(fifo_path_.c_str());
                return false;
            }
            if (pid == 0) {
                execlp("wl-paste", "wl-paste",
                       "--type", "text",
                       "--no-newline",
                       "--watch", self_exe.c_str(), "--wlclip-notify", fifo_path_.c_str(),
                       (char *) nullptr);
                _exit(127); // exec 失败(例如没装 wl-clipboard)
            }
            paste_pid_ = pid;
            callback_ = std::move(cb);
            worker_ = std::thread([this] { reader_loop(); });
            return true;
        }

        void stop() {
            if (!running_.exchange(false)) return;
            if (paste_pid_ > 0) {
                kill(paste_pid_, SIGTERM);
                int status;
                waitpid(paste_pid_, &status, 0);
                paste_pid_ = -1;
            }
            if (worker_.joinable()) worker_.join();
            if (!fifo_path_.empty()) {
                unlink(fifo_path_.c_str());
                fifo_path_.clear();
            }
        }

    private:
        static std::string &self_argv0() {
            static std::string v;
            return v;
        }

        static std::string resolve_self_exe() {
            char buf[4096];
            ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                return std::string(buf);
            }
            const std::string &a0 = self_argv0();
            if (!a0.empty()) {
                char resolved[4096];
                if (realpath(a0.c_str(), resolved)) {
                    return std::string(resolved);
                }
            }
            return {};
        }

        static void relay_stdin_to_fifo(const std::string &fifo_path) {
            std::string data;
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
                data.append(buf, n);
            }
            int fd = open(fifo_path.c_str(), O_WRONLY);
            if (fd < 0) return; // 主进程可能已退出, 静默放弃
            data.push_back('\0'); // 消息分隔符
            size_t off = 0;
            while (off < data.size()) {
                ssize_t w = write(fd, data.data() + off, data.size() - off);
                if (w <= 0) break;
                off += static_cast<size_t>(w);
            }
            close(fd);
        }

        void reader_loop() {
            int fd = open(fifo_path_.c_str(), O_RDWR | O_NONBLOCK);
            if (fd < 0) return;

            std::string buffer;
            char chunk[4096];
            while (running_) {
                struct pollfd pfd{fd, POLLIN, 0};
                int pr = poll(&pfd, 1, 500);
                if (pr <= 0) continue;

                if (pfd.revents & POLLIN) {
                    while (true) {
                        ssize_t n = read(fd, chunk, sizeof(chunk));
                        if (n > 0) {
                            buffer.append(chunk, static_cast<size_t>(n));
                        } else {
                            break;
                        }
                    }

                    size_t pos;
                    while ((pos = buffer.find('\0')) != std::string::npos) {
                        std::string text = buffer.substr(0, pos);
                        buffer.erase(0, pos + 1);
                        handle_change(text);
                    }
                }
            }
            close(fd);
        }

        void handle_change(const std::string &text) {
            {
                std::lock_guard lock(self_mutex_);
                if (skip_next_matching_ && text == last_self_text_) {
                    skip_next_matching_ = false;
                    return;
                }
            }
            if (callback_) callback_(text);
        }

        pid_t paste_pid_ = -1;
        std::string fifo_path_;
        std::thread worker_;
        std::atomic<bool> running_{false};
        ChangeCallback callback_;

        std::mutex self_mutex_;
        std::string last_self_text_;
        bool skip_next_matching_ = false;
    };
}
