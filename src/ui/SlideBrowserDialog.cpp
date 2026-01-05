#include "SlideBrowserDialog.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include <algorithm>
#include <cctype>

namespace pathview {
namespace ui {

SlideBrowserDialog::SlideBrowserDialog() = default;
SlideBrowserDialog::~SlideBrowserDialog() = default;

void SlideBrowserDialog::Open(std::shared_ptr<remote::WsiStreamClient> client) {
    client_ = std::move(client);
    isOpen_ = true;
    selectedIndex_ = -1;
    searchFilter_[0] = '\0';
    hasError_ = false;
    errorMessage_.clear();

    // Fetch slides
    FetchSlides();
}

void SlideBrowserDialog::Close() {
    isOpen_ = false;
    client_.reset();
    slides_.clear();
    filteredIndices_.clear();
}

void SlideBrowserDialog::SetOnSlideSelectedCallback(OnSlideSelectedCallback callback) {
    onSlideSelectedCallback_ = std::move(callback);
}

void SlideBrowserDialog::FetchSlides() {
    if (!client_ || !client_->IsConnected()) {
        hasError_ = true;
        errorMessage_ = "Not connected to server";
        return;
    }

    isLoading_ = true;

    auto result = client_->FetchSlideList();
    isLoading_ = false;

    if (result) {
        slides_ = std::move(*result);
        FilterSlides();
    } else {
        hasError_ = true;
        errorMessage_ = client_->GetLastError();
    }
}

void SlideBrowserDialog::FilterSlides() {
    filteredIndices_.clear();

    std::string filter(searchFilter_);
    // Convert to lowercase for case-insensitive search
    std::transform(filter.begin(), filter.end(), filter.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (size_t i = 0; i < slides_.size(); ++i) {
        if (filter.empty()) {
            filteredIndices_.push_back(i);
        } else {
            std::string name = slides_[i].name;
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (name.find(filter) != std::string::npos) {
                filteredIndices_.push_back(i);
            }
        }
    }

    // Reset selection if it's no longer visible
    if (selectedIndex_ >= 0 &&
        std::find(filteredIndices_.begin(), filteredIndices_.end(),
                  static_cast<size_t>(selectedIndex_)) == filteredIndices_.end()) {
        selectedIndex_ = -1;
    }
}

void SlideBrowserDialog::Render() {
    if (!isOpen_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Select Slide", &isOpen_, flags)) {
        // Search bar
        ImGui::Text("%s Search:", ICON_FA_MAGNIFYING_GLASS);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-80);
        if (ImGui::InputText("##Search", searchFilter_, sizeof(searchFilter_))) {
            FilterSlides();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            searchFilter_[0] = '\0';
            FilterSlides();
        }

        ImGui::Separator();

        // Error message
        if (hasError_) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s %s", ICON_FA_CIRCLE_EXCLAMATION, errorMessage_.c_str());
            ImGui::PopStyleColor();
        }

        // Loading indicator
        if (isLoading_) {
            ImGui::Text("Loading slides...");
        }

        // Slide list
        float footerHeight = ImGui::GetFrameHeightWithSpacing() * 3 + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("SlideList", ImVec2(0, -footerHeight), true)) {
            // Table header
            if (ImGui::BeginTable("SlidesTable", 2,
                                   ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_BordersInnerV |
                                   ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                for (size_t i : filteredIndices_) {
                    const auto& slide = slides_[i];

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    bool isSelected = (selectedIndex_ == static_cast<int>(i));
                    if (ImGui::Selectable(slide.name.c_str(), isSelected,
                                          ImGuiSelectableFlags_SpanAllColumns |
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                        selectedIndex_ = static_cast<int>(i);

                        // Double-click to open
                        if (ImGui::IsMouseDoubleClicked(0) && onSlideSelectedCallback_) {
                            onSlideSelectedCallback_(slide.id);
                            Close();
                        }
                    }

                    ImGui::TableNextColumn();
                    if (slide.size > 0) {
                        // Format size
                        if (slide.size >= 1024 * 1024 * 1024) {
                            ImGui::Text("%.1f GB", slide.size / (1024.0 * 1024.0 * 1024.0));
                        } else if (slide.size >= 1024 * 1024) {
                            ImGui::Text("%.1f MB", slide.size / (1024.0 * 1024.0));
                        } else if (slide.size >= 1024) {
                            ImGui::Text("%.1f KB", slide.size / 1024.0);
                        } else {
                            ImGui::Text("%lld B", static_cast<long long>(slide.size));
                        }
                    } else {
                        ImGui::TextDisabled("-");
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        // Selected slide info
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(slides_.size())) {
            const auto& slide = slides_[selectedIndex_];
            ImGui::Text("Selected: %s", slide.name.c_str());
        } else {
            ImGui::TextDisabled("No slide selected");
        }

        ImGui::Spacing();

        // Buttons
        float buttonWidth = 100.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = buttonWidth * 2 + spacing;
        float startX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);

        // Open button
        bool canOpen = selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(slides_.size());
        if (!canOpen) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open", ImVec2(buttonWidth, 0))) {
            if (onSlideSelectedCallback_) {
                onSlideSelectedCallback_(slides_[selectedIndex_].id);
            }
            Close();
        }
        if (!canOpen) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            Close();
        }
    }
    ImGui::End();
}

}  // namespace ui
}  // namespace pathview
