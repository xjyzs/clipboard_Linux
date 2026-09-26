#include <main.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <sio_client.h>
#include <fstream>
#include <string>

#include "utils/clipboard.hpp"

using namespace std;

static atomic g_running{true};
static void on_sigint(int) { g_running = false; }
static cb::Clipboard clip;
static sio::client sio_client;
static string url;

static void on_connect() {
    std::cout << "连接成功\n";
}

static void on_disconnect(sio::client::close_reason const &reason) {
    std::cout << "已断开\n";
}

static void set_url(const string &newUrl) {
    url = newUrl;
    FILE *f = fopen("url.txt", "w");
    if (f != nullptr) {
        fputs(newUrl.c_str(), f);
        fclose(f);
    }
    thread([newUrl]() {
        try {
            sio_client.sync_close();
            sio_client.connect(url);
        } catch (const exception &e) {
        }
    }).detach();
}

int main(const int argc, char **argv) {
    if (cb::Clipboard::bootstrap(argc, argv)) {
        return 0;
    }
    signal(SIGINT, on_sigint);

    auto ui = MainWindow::create();
    slint::ComponentWeakHandle<MainWindow> ui_weak(ui);

    ifstream url_file("url.txt");
    if (url_file.is_open() && getline(url_file, url) && !url.empty()) {
        url_file.close();
    } else {
        url_file.close();
        ui->set_show_url_alert(true);
    }

    sio_client.set_open_listener(on_connect);
    sio_client.set_close_listener(on_disconnect);
    sio_client.set_logs_quiet();

    sio_client.socket()->on("message", [ui_weak](const sio::event &ev) {
        const auto &msg = ev.get_message();
        if (msg && msg->get_flag() == sio::message::flag_string) {
            std::string data = msg->get_string();
            slint::invoke_from_event_loop([ui_weak, data]() {
                if (auto ui = ui_weak.lock()) {
                    (*ui)->set_clipText(data.data());
                    clip.copy(data);
                }
            });
        }
    });

    try {
        sio_client.connect(url);
    } catch (const exception &e) {
    }
    // 首次不发送剪贴板内容
    bool isFirstLaunch = true;
    bool ok = clip.watch([&isFirstLaunch](const string &text) {
        if (!isFirstLaunch)sio_client.socket()->emit("message", text);
        else isFirstLaunch = false;
    });
    if (!ok) ui->set_show_alert(true);

    ui->on_submit([](const slint::SharedString &s) {
        sio_client.socket()->emit("message", (string) s);
    });

    ui->on_setUrl([](const slint::SharedString &s) {
        set_url((string) s);
    });

    ui->on_setUrl([](const slint::SharedString &s) {
        clip.copy((string) s);
    });

    ui->run();
    return 0;
}
