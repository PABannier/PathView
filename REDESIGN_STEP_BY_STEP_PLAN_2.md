# PathView UI Redesign - Step-by-Step Plan

This plan outlines the steps to implement the UI redesign defined in `REDESIGN_PLAN.md`.

## Phase 1: Foundation & Styling

### 1.1. Update UI Styling (`src/core/UIStyle.h`, `src/core/UIStyle.cpp`)
- **Objective:** Establish the "calm, medical, trustworthy" aesthetic.
- **Tasks:**
    - Update `UIStyle::Colors` to match the new palette (refined sans-serif, increased line-height, consistent accent color).
    - Adjust `ImGuiStyle` variables (padding, rounding, borders) to remove harsh contrasts and developer-centric look.
    - Define new semantic color variables (e.g., `Accent`, `Selection`, `Focus`, `Background`, `Surface`).

### 1.2. Refactor Application State for UI (`src/core/Application.h`)
- **Objective:** Support new interaction modes and contextual UI.
- **Tasks:**
    - Define `InteractionMode` enum: `Navigate`, `Annotate`, `Analyze`.
    - Add `currentMode_` member variable to `Application` class.
    - Add `selectedLayer_` or similar state for the Sidebar "Active Selection" section.
    - Remove `RenderMenuBar` (merge into Top Bar or native OS menu if possible, but for now, we'll keep a minimal menu bar or integrate it). *Correction:* The plan says "Left (App-Level): File, View, Help", "Center: Modes", "Right: Contextual". So we likely need a custom Top Bar that acts as the main navigation, possibly replacing or augmenting the standard ImGui `MainMenuBar`.

## Phase 2: Top Bar & Interaction Modes

### 2.1. Implement New Top Bar (`src/core/Application.cpp`)
- **Objective:** Create the structured top bar with modes.
- **Tasks:**
    - Replace `RenderToolbar` with `RenderTopBar`.
    - **Left Section:** "File", "View", "Help" menus (styled as buttons/dropdowns).
    - **Center Section:** Implement a segmented control for `Navigate`, `Annotate`, `Analyze`.
        - Logic: Switching modes updates `currentMode_`.
    - **Right Section:** Contextual controls (Reset View, Sidebar Toggle, Server Status).
    - **Context Awareness:** Ensure only relevant tools appear based on `currentMode_`.

### 2.2. Mode-Specific Tool Logic
- **Objective:** Bind UI modes to actual application logic.
- **Tasks:**
    - **Navigate Mode:** Default state. Pan/Zoom enabled. Annotation tools disabled.
    - **Annotate Mode:** Show "Polygon Tool" (and future annotation tools) in a sub-toolbar or context menu.
    - **Analyze Mode:** Show measurement/analysis tools (placeholder for now if not fully implemented).

## Phase 3: Empty State & Canvas

### 3.1. Redesign Empty State (`src/core/Application.cpp`)
- **Objective:** Create a full-bleed, professional first impression.
- **Tasks:**
    - Rewrite `RenderWelcomeOverlay` to `RenderEmptyState`.
    - Remove window centering/sizing constraints to make it full-screen (within the viewport area).
    - Render:
        - Large headline: "Explore whole-slide pathology at cellular resolution".
        - Primary CTA: "Open slide" (large button).
        - Secondary CTA: "Connect to server".
        - Background: Subtle grid or texture (draw using `ImGui::GetWindowDrawList()`).

### 3.2. Canvas Enhancements
- **Objective:** Add "precision instrument" feel.
- **Tasks:**
    - Implement `RenderScaleBar` in `Application.cpp`.
        - Calculate physical width based on `viewport_->GetZoom()` and slide microns-per-pixel (if available) or default.
        - Draw line and text label (e.g., "100 µm") in the bottom-left or bottom-right.
    - Add `RenderTooltips` logic for onboarding (dismissible).

## Phase 4: Sidebar Redesign

### 4.1. Refactor Sidebar Structure (`src/core/Application.cpp`)
- **Objective:** Move from Tabs to Stacked Contextual Sections.
- **Tasks:**
    - Rewrite `RenderSidebar`.
    - Remove `ImGui::BeginTabBar`.
    - Implement stacked sections using `ImGui::CollapsingHeader` (always open or fixed distinct regions).

### 4.2. Implement Sidebar Sections
- **Tasks:**
    - **Section 1: Slide Overview:**
        - Show Thumbnail (if available/implementable), Stain, Magnification, Dimensions.
    - **Section 2: Active Selection (Contextual):**
        - Only visible when an annotation or polygon is selected.
        - Show Area, Cell counts, Actions (Analyze, Delete).
    - **Section 3: Layers:**
        - Replace "Polygon Tab" logic.
        - List layers: "Slide", "Annotations", "Cell Detections" (previously "Cell Polygons").
        - Each layer has: Visibility toggle (eye), Opacity slider, Color chip.

## Phase 5: Polish & Onboarding

### 5.1. Naming & Language
- **Objective:** Use product language.
- **Tasks:**
    - Rename "Cell Polygons" -> "Cell detections".
    - Rename "Polygon Tool" -> "Annotation tool".
    - Review all tooltips and labels.

### 5.2. Visual Polish
- **Objective:** Final fit and finish.
- **Tasks:**
    - Verify typography hierarchy (headers vs body).
    - Ensure consistent padding/spacing.
    - Test empty states for all panels.
    - Check "Visual calm" (reduce noise).

## Execution Order
1.  **Step 1:** Styles & Empty State (Phases 1.1, 3.1). *High impact, establishes look.*
2.  **Step 2:** Top Bar & Modes (Phases 1.2, 2.1, 2.2). *Core navigation structure.*
3.  **Step 3:** Sidebar & Layers (Phases 4.1, 4.2). *Refines content organization.*
4.  **Step 4:** Canvas & Details (Phases 3.2, 5.1, 5.2). *Final polish.*
