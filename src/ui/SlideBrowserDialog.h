#pragma once

#include "../remote/WsiStreamClient.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace pathview {
namespace ui {

class SlideBrowserDialog {
public:
    // Callback when a slide is selected
    using OnSlideSelectedCallback = std::function<void(const std::string& slideId)>;

    SlideBrowserDialog();
    ~SlideBrowserDialog();

    // Show the dialog with slides from a connected client
    void Open(std::shared_ptr<remote::WsiStreamClient> client);
    void Close();
    bool IsOpen() const { return isOpen_; }

    // Render the dialog (call each frame)
    void Render();

    // Set callback for slide selection
    void SetOnSlideSelectedCallback(OnSlideSelectedCallback callback);

private:
    void FetchSlides();
    void FilterSlides();

    bool isOpen_ = false;

    // Client connection
    std::shared_ptr<remote::WsiStreamClient> client_;

    // Slide list
    std::vector<remote::SlideEntry> slides_;
    std::vector<size_t> filteredIndices_;  // Indices into slides_ matching filter
    int selectedIndex_ = -1;

    // Search/filter
    char searchFilter_[256] = "";

    // State
    bool isLoading_ = false;
    bool hasError_ = false;
    std::string errorMessage_;

    // Callback
    OnSlideSelectedCallback onSlideSelectedCallback_;
};

}  // namespace ui
}  // namespace pathview
