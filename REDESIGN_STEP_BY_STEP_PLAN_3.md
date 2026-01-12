# PathView UI Redesign - Step-by-Step Implementation Plan

This plan translates the design requirements from `REDESIGN_PLAN.md` into concrete implementation steps for the PathView codebase.

---

## Overview

**Current Architecture:**
- Framework: ImGui 1.90.0 + SDL2 (C++17)
- Main class: `Application` (~2,200 LOC)
- UI Styling: `UIStyle.h/cpp`
- Key files to modify:
  - `src/core/Application.h/cpp` - Main UI rendering
  - `src/core/UIStyle.h/cpp` - Colors, fonts, spacing
  - New files for modular components

**Redesign Scope:**
1. Empty state redesign
2. Interaction mode system (Navigate, Annotate, Analyze)
3. Contextual sidebar with stacked sections
4. Canvas enhancements (scale bar)
5. Tool discoverability (tooltips, hints)
6. Visual language refinement
7. Product language naming

---

## Phase 1: Foundation - Visual Language & Styling

### Step 1.1: Refine Color Palette (UIStyle.h)
- Replace pure black backgrounds with warmer dark tones
- Define single accent color for active states, selections, focus
- Add subtle background texture color option
- Colors to update:
  ```cpp
  MAIN_BG        // Change from #111318 to warmer #15171C
  PANEL_BG       // Adjust for subtle depth
  ACCENT         // Keep #4EA6FF as primary accent
  ACCENT_HOVER   // Lighter variant
  ACCENT_ACTIVE  // Darker variant
  ```

### Step 1.2: Typography Improvements (UIStyle.cpp)
- Increase line-height globally (1.4x minimum)
- Ensure consistent font usage (Inter only)
- Define font size scale:
  - Body: 14px
  - UI Labels: 13px
  - Headings: 16px
  - Large headings: 20px

### Step 1.3: Spacing System
- Define spacing constants:
  ```cpp
  SPACE_XS = 4px
  SPACE_SM = 8px
  SPACE_MD = 12px
  SPACE_LG = 16px
  SPACE_XL = 24px
  ```
- Apply consistent padding/margins throughout UI

**Files:** `src/core/UIStyle.h`, `src/core/UIStyle.cpp`

---

## Phase 2: Interaction Mode System

### Step 2.1: Define Mode Enum and State
Create new header `src/core/InteractionMode.h`:
```cpp
enum class InteractionMode {
    Navigate,   // Default: pan, zoom, explore
    Annotate,   // ROI creation, polygon drawing
    Analyze     // Measurement tools, cell counting
};
```

Add to Application class:
- `InteractionMode currentMode_`
- `void SetInteractionMode(InteractionMode mode)`
- `InteractionMode GetInteractionMode() const`

### Step 2.2: Refactor Toolbar with Mode Selector
Replace current flat button toolbar with:

**Left Section (App-Level):**
- File menu button
- View menu button
- Help menu button

**Center Section (Mode Selector - Segmented Control):**
- Navigate (default)
- Annotate
- Analyze

Implement as **segmented control** (pill-shaped buttons like iOS/macOS) using custom ImGui styling with clear active state highlighting.

**Right Section (Contextual + Utilities):**
- Reset View (icon-only)
- Toggle Sidebar (icon-only)
- Connection status indicator

### Step 2.3: Mode-Specific Tool Visibility
Create helper method `RenderModeTools()`:
- **Navigate mode:** No additional tools
- **Annotate mode:** Show polygon tool, shape tools
- **Analyze mode:** Show measurement tools, cell count trigger

### Step 2.4: Update Event Handling
Modify `ProcessEvents()`:
- Route mouse/keyboard events based on `currentMode_`
- Only allow annotation drawing in Annotate mode
- Only allow measurement tools in Analyze mode
- Navigation (pan/zoom) available in all modes

**Files:** `src/core/InteractionMode.h` (new), `src/core/Application.h`, `src/core/Application.cpp`

---

## Phase 3: Empty State Redesign

### Step 3.1: Remove Modal Welcome Overlay
- Delete current `RenderWelcomeOverlay()` modal card implementation
- Remove floating card styling

### Step 3.2: Create Full-Bleed Empty State
New method `RenderEmptyState()`:

**Visual Layout:**
- Centered content (vertically and horizontally)
- Subtle microscopy-inspired grid background
- No modal/card container

**Content:**
```
[Large headline - 24px]
"Explore whole-slide pathology at cellular resolution"

[Primary CTA - prominent button]
"Open Slide" (Ctrl+O)

[Secondary CTA - text link style]
"Connect to server"

[Subtle footer]
Keyboard shortcuts hint
```

### Step 3.3: Background Pattern
- Implement subtle dot grid or microscopy-inspired pattern
- Render as SDL texture or ImGui DrawList
- Very low opacity (5-10%)

**Files:** `src/core/Application.cpp`

---

## Phase 4: Contextual Sidebar Redesign

### Step 4.1: Remove Tab System
- Delete `ImGui::BeginTabBar` / `ImGui::BeginTabItem` calls
- Replace with stacked sections using collapsible headers

### Step 4.2: Section 1 - Slide Overview (Always Visible)
Create `RenderSlideOverviewSection()`:
- Slide thumbnail (small preview)
- Metadata table:
  - Stain type (if available)
  - Magnification
  - Dimensions (W x H)
  - Scanner/source
- Collapsible header: "Slide Overview"

### Step 4.3: Section 2 - Active Selection (Contextual)
Create `RenderActiveSelectionSection()`:
- Only visible when annotation/ROI is selected
- Track selection state: `selectedAnnotationId_`
- Display:
  - Selection name (editable)
  - Area calculation
  - Cell counts by class
  - Actions: Analyze, Export, Delete
- Collapsible header: "Selection"

### Step 4.4: Section 3 - Layers (Always Visible)
Create `RenderLayersSection()`:

Implement Figma-style layer list:
```
[Eye icon] [Color chip] Slide        [Opacity slider]
[Eye icon] [Color chip] Annotations  [Opacity slider]
[Eye icon] [Color chip] Cell detections [Opacity slider]
[Eye icon] [Color chip] Heatmaps     [Opacity slider]
```

Each layer row:
- Visibility toggle (eye icon)
- Color indicator
- Layer name
- Opacity slider (inline)

Create `Layer` abstraction:
```cpp
struct Layer {
    std::string name;
    bool visible;
    float opacity;
    ImVec4 color;
    LayerType type;
};
```

### Step 4.5: Integrate Sections
New `RenderSidebar()`:
```cpp
void RenderSidebar() {
    RenderSlideOverviewSection();   // Always visible
    RenderActiveSelectionSection(); // Contextual
    RenderLayersSection();          // Always visible
}
```

**Files:** `src/core/Application.cpp`, `src/core/Layer.h` (new)

---

## Phase 5: Canvas Enhancements

### Step 5.1: Scale Bar Implementation
Create `ScaleBar` class or method:
- Position: **Bottom-left corner**
- Shows current scale in micrometers (µm)
- Dynamic: updates with zoom level
- Styled to match UI (subtle, non-intrusive)

Implementation:
```cpp
void RenderScaleBar() {
    float pixelsPerMicron = calculatePixelsPerMicron(viewport_->GetZoom());
    float barLengthMicrons = selectNiceScaleValue(pixelsPerMicron);
    // Render bar with label: "100 µm", "50 µm", etc.
}
```

### Step 5.2: Visual Depth
- Add subtle vignette or edge shadow to canvas
- Consider very subtle grid at high zoom (optional)

**Files:** `src/core/Application.cpp` or `src/core/ScaleBar.h` (new)

---

## Phase 6: Tool Discoverability & Onboarding

### Step 6.1: First-Use Tooltip System
Create `OnboardingTooltips` class:
- Track dismissed tooltips in preferences/file
- Show once per feature:
  - "Scroll to zoom"
  - "Click + drag to pan"
  - "Press A to start annotation"
- Position near relevant UI element
- Dismissible with "Got it" button

### Step 6.2: Hover Hints During Annotation
- Show contextual hints near cursor:
  - "Click to add point"
  - "Double-click to close polygon"
  - "Press Escape to cancel"
- Render as small label following cursor

### Step 6.3: Inline Helper Text
- Add subtle helper text below tool buttons
- Example: "Draw regions of interest"

**Files:** `src/core/OnboardingTooltips.h` (new), `src/core/Application.cpp`

---

## Phase 7: Naming & Product Language

### Step 7.1: Audit Current Labels
Find and replace:
- "Cell Poly..." → "Cell detections"
- "Polygon Tool" → "Draw Region" or "Annotate"
- "Action Cards" → "Tasks" or "Activity"
- "Polygons" → "Annotations"

### Step 7.2: Menu Updates
- Review all menu item labels
- Ensure consistency with product language

### Step 7.3: Tooltip and Help Text
- Update all tooltips with clear, user-friendly language
- Remove developer jargon

**Files:** All UI rendering code in `src/core/Application.cpp`

---

## Phase 8: Component Extraction & Refactoring

### Step 8.1: Extract Reusable Components
Create modular UI components:
- `src/ui/ModeSelector.h/cpp` - Segmented control for modes
- `src/ui/LayerPanel.h/cpp` - Layer list component
- `src/ui/SelectionPanel.h/cpp` - Active selection panel
- `src/ui/ScaleBar.h/cpp` - Scale bar component

### Step 8.2: Clean Up Application Class
- Move UI rendering to separate methods
- Consider `UIRenderer` class to reduce Application size
- Maintain separation: rendering vs. state management

**Files:** New files in `src/ui/`

---

## Implementation Order (Recommended)

1. **Phase 1** - Foundation (styling) - Low risk, immediate visual improvement
2. **Phase 2** - Mode system - Core architectural change
3. **Phase 3** - Empty state - High-impact first impression
4. **Phase 4** - Sidebar - Major UI restructure
5. **Phase 5** - Canvas (scale bar) - Focused enhancement
6. **Phase 6** - Onboarding - Polish layer
7. **Phase 7** - Naming - Throughout implementation
8. **Phase 8** - Refactoring - Continuous as we go

---

## Verification Plan

### Visual Testing
- Screenshot comparison before/after each phase
- Test at different window sizes
- Test with/without slide loaded
- Test high-DPI displays

### Functional Testing
- All existing features still work
- Mode switching works correctly
- Sidebar sections show/hide appropriately
- Scale bar updates with zoom
- Keyboard shortcuts preserved

### User Flow Testing
- First-time user can open slide in <10 seconds
- Annotation workflow is intuitive
- Power features are discoverable

---

## Files Summary

**Modify:**
- `src/core/Application.h` - Add mode state, selection state
- `src/core/Application.cpp` - Major UI rendering changes
- `src/core/UIStyle.h` - Color palette, spacing constants
- `src/core/UIStyle.cpp` - Typography, styling application

**Create:**
- `src/core/InteractionMode.h` - Mode enum and utilities
- `src/core/Layer.h` - Layer abstraction
- `src/core/ScaleBar.h/cpp` - Scale bar component
- `src/core/OnboardingTooltips.h/cpp` - First-use hints
- `src/ui/ModeSelector.h/cpp` - Mode selector component
- `src/ui/LayerPanel.h/cpp` - Layer panel component
- `src/ui/SelectionPanel.h/cpp` - Selection panel component

---

## Notes

- Keep performance-critical rendering paths (SlideRenderer, PolygonOverlay) untouched
- Maintain all existing keyboard shortcuts
- Preserve IPC/MCP functionality for AI agent control
- Test remote slide loading after changes
