// Copyright (c) 2017 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cef_application_mac.h"
#include "include/wrapper/cef_helpers.h"

#include "include/cef_app.h"
#include "include/wrapper/cef_library_loader.h"

#include <iostream>
#include <thread>

// Minimal implementation of client handlers.
class Client : public CefClient,
               public CefDisplayHandler,
               public CefLifeSpanHandler,
               public CefResourceRequestHandler {
 public:
  Client() {}

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE { return this; }

 private:
  IMPLEMENT_REFCOUNTING(Client);
  DISALLOW_COPY_AND_ASSIGN(Client);
};

// const char kStartupURL[] = "https://www.google.com";
const char kStartupURL[] = "about:blank";

// Minimal implementation of CefApp for the browser process.
class BrowserApp : public CefApp, public CefBrowserProcessHandler {
 public:
  BrowserApp(CefRefPtr<CefBrowser> _browser) : browser(_browser) {}

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() OVERRIDE {
    return this;
  }

  CefRefPtr<CefBrowser> browser;
 private:
  IMPLEMENT_REFCOUNTING(BrowserApp);
  DISALLOW_COPY_AND_ASSIGN(BrowserApp);
};

CefRefPtr<BrowserApp> MyCreateBrowserProcessApp() {
  CefWindowInfo window_info;
  auto browser =
      CefBrowserHost::CreateBrowserSync(window_info, new Client(), kStartupURL,
                                        CefBrowserSettings(), nullptr, nullptr);
  return new BrowserApp(browser);
}

// Implementation of CefApp for the renderer process.
class RendererApp : public CefApp, public CefRenderProcessHandler {
 public:
  RendererApp() {}

  // CefApp methods:
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() OVERRIDE {
    return this;
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) OVERRIDE {
    std::cout << "Renderer -> Message Received: " << message->GetName()
              << std::endl;
    return true;
  }

  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) OVERRIDE {
    // Command-line flags can be modified in this callback.
    // |process_type| is empty for the browser process.
    if (process_type.empty()) {
#if defined(OS_MACOSX)
      // Disable the macOS keychain prompt. Cookies will not be encrypted.
      command_line->AppendSwitch("use-mock-keychain");
#endif
      command_line->AppendSwitch("single-process");
    }
  }

 private:
  IMPLEMENT_REFCOUNTING(RendererApp);
  DISALLOW_COPY_AND_ASSIGN(RendererApp);
};

CefRefPtr<CefApp> MyCreateRendererProcessApp() {
  return new RendererApp();
}


namespace shared {
// Dummy implementations to avoid linkare errors
CefRefPtr<CefApp> CreateBrowserProcessApp() {
  return NULL;
}
CefRefPtr<CefApp> CreateOtherProcessApp() {
  return NULL;
}

CefRefPtr<CefApp> CreateRendererProcessApp() {
  return NULL;
}
}

// Entry point function for the browser process.
int main(int argc, char* argv[]) {
  // Load the CEF framework library at runtime instead of linking directly
  // as required by the macOS sandbox implementation.
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain())
    return 1;

  // Provide CEF with command-line arguments.
  CefMainArgs main_args(argc, argv);

  // Specify CEF global settings here.
  CefSettings settings;

  CefRefPtr<CefApp> renderer = MyCreateRendererProcessApp();

  CefExecuteProcess(main_args, renderer, NULL);
  CefInitialize(main_args, settings, renderer, NULL);

  // Create a CefApp for the browser process. Other processes are handled by
  // process_helper_mac.cc.
  auto app = MyCreateBrowserProcessApp();

  // Initialize CEF for the browser process. The first browser instance will be
  // created in CefBrowserProcessHandler::OnContextInitialized() after CEF has
  // been initialized.
  CefInitialize(main_args, settings, app, NULL);

  auto f = [app]() {
    while (true) {
      std::cout << "Enter message: " << std::endl;;
      std::string s;
      std::cin >> s;
      std::cout << "Sending message: "
                << "'" << s << "'" << std::endl;
      CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(s);
      app->browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, msg);
    }
  };

  std::thread t(f);
  std::cout << "Running CEF Message Loop" << std::endl;
  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();
  t.join();

  // Shut down CEF.
  CefShutdown();
  return 0;
}
