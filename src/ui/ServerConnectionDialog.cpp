#include "ServerConnectionDialog.h"
#include "../remote/WsiStreamClient.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include <thread>

namespace pathview {
namespace ui {

ServerConnectionDialog::ServerConnectionDialog() = default;
ServerConnectionDialog::~ServerConnectionDialog() = default;

void ServerConnectionDialog::Open() {
    isOpen_ = true;
    ResetState();
}

void ServerConnectionDialog::Close() {
    isOpen_ = false;
    isConnecting_ = false;
}

void ServerConnectionDialog::ResetState() {
    hasError_ = false;
    errorMessage_.clear();
    serverVersion_.clear();
    connectedClient_.reset();
}

void ServerConnectionDialog::SetOnConnectedCallback(OnConnectedCallback callback) {
    onConnectedCallback_ = std::move(callback);
}

void ServerConnectionDialog::Render() {
    if (!isOpen_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("Connect to WsiStreamer Server", &isOpen_, flags)) {
        // Server URL input
        ImGui::Text("Server URL:");
        ImGui::SetNextItemWidth(-1);
        bool enterPressed = ImGui::InputText("##ServerUrl", serverUrl_, sizeof(serverUrl_),
                                              ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        // Auth secret input (password field)
        ImGui::Text("Auth Secret (optional):");
        ImGui::SetNextItemWidth(-1);
        enterPressed |= ImGui::InputText("##AuthSecret", authSecret_, sizeof(authSecret_),
                                          ImGuiInputTextFlags_Password |
                                          ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Error message
        if (hasError_) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s %s", ICON_FA_CIRCLE_EXCLAMATION, errorMessage_.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Success message
        if (!serverVersion_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("%s Connected (v%s)", ICON_FA_CHECK_CIRCLE, serverVersion_.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Buttons
        float buttonWidth = 120.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = buttonWidth * 2 + spacing;
        float startX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);

        // Connect button
        if (isConnecting_) {
            ImGui::BeginDisabled();
            ImGui::Button("Connecting...", ImVec2(buttonWidth, 0));
            ImGui::EndDisabled();
        } else {
            if (ImGui::Button(ICON_FA_PLUG " Connect", ImVec2(buttonWidth, 0)) || enterPressed) {
                AttemptConnection();
            }
        }

        ImGui::SameLine();

        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            Close();
        }
    }
    ImGui::End();
}

void ServerConnectionDialog::AttemptConnection() {
    if (isConnecting_) {
        return;
    }

    ResetState();
    isConnecting_ = true;

    // Create client and attempt connection
    auto client = std::make_shared<remote::WsiStreamClient>();
    std::string url = serverUrl_;
    std::string secret = authSecret_;

    auto result = client->Connect(url, secret);

    isConnecting_ = false;

    if (result.success) {
        serverVersion_ = result.serverVersion;
        connectedClient_ = client;

        // Invoke callback and close dialog
        if (onConnectedCallback_) {
            onConnectedCallback_(connectedClient_);
        }
        Close();
    } else {
        hasError_ = true;
        errorMessage_ = result.errorMessage;
    }
}

}  // namespace ui
}  // namespace pathview
