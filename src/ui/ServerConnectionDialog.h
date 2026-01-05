#pragma once

#include <string>
#include <functional>
#include <memory>

namespace pathview {
namespace remote {
class WsiStreamClient;
}
}

namespace pathview {
namespace ui {

class ServerConnectionDialog {
public:
    // Callback when connection is successful
    using OnConnectedCallback = std::function<void(std::shared_ptr<remote::WsiStreamClient>)>;

    ServerConnectionDialog();
    ~ServerConnectionDialog();

    // Show/hide the dialog
    void Open();
    void Close();
    bool IsOpen() const { return isOpen_; }

    // Render the dialog (call each frame)
    void Render();

    // Set callback for successful connection
    void SetOnConnectedCallback(OnConnectedCallback callback);

private:
    void AttemptConnection();
    void ResetState();

    bool isOpen_ = false;

    // Input fields
    char serverUrl_[256] = "http://localhost:3000";
    char authSecret_[256] = "";

    // Connection state
    bool isConnecting_ = false;
    bool hasError_ = false;
    std::string errorMessage_;
    std::string serverVersion_;

    // Result
    std::shared_ptr<remote::WsiStreamClient> connectedClient_;
    OnConnectedCallback onConnectedCallback_;
};

}  // namespace ui
}  // namespace pathview
