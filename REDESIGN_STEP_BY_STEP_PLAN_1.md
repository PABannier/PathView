# PathView UI Redesign - Step-by-Step Plan 1

## Codebase map (UI-relevant)
- `src/core/Application.cpp` / `src/core/Application.h`: main UI composition (`RenderMenuBar`, `RenderToolbar`, `RenderSidebar`, `RenderWelcomeOverlay`), input handling, and layout sizing.
- `src/core/UIStyle.cpp` / `src/core/UIStyle.h`: ImGui theme, colors, spacing, rounding.
- `src/core/AnnotationManager.cpp` / `src/core/AnnotationManager.h`: polygon annotation tool, drawing preview, and annotation list UI.
- `src/core/PolygonOverlay.cpp` / `src/core/PolygonOverlay.h`: cell detection overlay, visibility, opacity, class colors.
- `src/core/Minimap.cpp` / `src/core/Minimap.h`: minimap overlay rendering and interaction.
- `src/ui/ServerConnectionDialog.cpp` / `src/ui/SlideBrowserDialog.cpp`: modal dialogs for server connection and slide selection.
- `src/core/Viewport.*`, `src/core/SlideLoader.*`, `src/remote/RemoteSlideSource.*`: viewport math and slide metadata sources that the UI needs to expose.

## Step-by-step plan
1. Establish the new UI architecture boundary.
   - Split the monolithic `Application::RenderUI()` into focused UI components (new `src/ui/*` classes or static helpers) for top bar, sidebar sections, empty state, scale bar, and onboarding hints.
   - Keep rendering and slide pipelines untouched; move only UI composition out of `Application.cpp`.

2. Add UI state models needed by the redesign.
   - Introduce an `InteractionMode` enum in `Application.h` (Navigate, Annotate, Analyze) and store it in `Application`.
   - Add a lightweight `SelectionState` (selected annotation/ROI id) and `LayerState` (visibility, opacity, color chip) to drive the contextual sidebar and layers list.

3. Replace the welcome modal with a full-bleed empty state.
   - Rework `RenderWelcomeOverlay()` into a full-screen, centered layout with headline, primary "Open slide" CTA, and secondary "Connect to server" CTA.
   - Draw a subtle microscopy-inspired background (grid or texture) using an ImGui draw list so the empty state feels deliberate.
   - Ensure this empty state is shown only when no local or remote slide is loaded.

4. Build the redesigned top bar with interaction modes.
   - Replace `RenderMenuBar()` + `RenderToolbar()` with a single `RenderTopBar()` layout: left app menus, center segmented control, right icon-only controls.
   - Update layout math in `UpdateViewportRect()` to reflect the new top bar height.
   - Keep keyboard shortcuts intact (Cmd/Ctrl+O, Cmd/Ctrl+P, etc).

5. Gate tools by interaction mode.
   - In Navigate mode, hide annotation and analysis tools.
   - In Annotate mode, show polygon tool and any annotation UI; toggle `AnnotationManager::SetToolActive()` when entering/exiting.
   - In Analyze mode, introduce a placeholder for measurement tools (even if the tools are stubs initially).

6. Replace sidebar tabs with stacked contextual sections.
   - Rebuild `RenderSidebar()` to render: (1) Slide Overview (always), (2) Active Selection (contextual), (3) Layers (always), plus a low-priority Activity section for action cards if needed.
   - Convert existing tab content into sections:
     - Slide info from `RenderSlideInfoTab()`.
     - Cell detections from `RenderPolygonTab()`.
     - Annotations from `AnnotationManager::RenderUI()`.
   - Remove the tab bar entirely once sections are in place.

7. Implement selection-aware UI for annotations.
   - Add selection tracking in `AnnotationManager` (click in list and optionally on-canvas).
   - Drive the "Active Selection" section with metrics: area, cell counts, and actions (Analyze, Export, Delete).
   - Keep destructive actions consistent with existing behavior (delete annotations, cancel drawing).

8. Introduce a Layers panel that mirrors a design tool.
   - Create a `LayerState` list for: Slide, Cell detections (polygon overlay), Annotations.
   - Hook visibility toggles to `PolygonOverlay::SetVisible()` and a new `AnnotationManager` visibility flag.
   - Add opacity controls and color chips for overlays (reuse `PolygonOverlay` class colors).

9. Add a scale bar overlay for physical grounding.
   - Implement a small `ScaleBar` renderer (ImGui or SDL overlay) drawn after slide rendering.
   - Extend `ISlideSource` to surface microns-per-pixel when available; implement for `SlideLoader` (OpenSlide properties) and `RemoteSlideSource` (from `SlideInfo` if available).
   - Hide or label the scale bar when metadata is missing.

10. Add onboarding and tool discoverability cues.
    - Add first-use hints for zoom, pan, and polygon drawing in `Application`.
    - Use transient ImGui overlays or tooltips that fade after interaction.
    - Keep hints contextual to the active mode to avoid noise.

11. Update visual language and copy to match the redesign brief.
    - Revise labels in `Application.cpp`, `AnnotationManager.cpp`, and dialogs to product language ("Cell detections", "Annotations", "Export and Analyze").
    - Update `UIStyle` to reduce contrast, increase line-height, and standardize the accent color for active/selection states.
    - Keep Inter as the base font unless a new font is introduced explicitly.

12. Verify behavior and layout integrity.
    - Confirm the new top bar and sidebar do not obstruct the viewport (check `UpdateViewportRect()` and minimap positioning).
    - Ensure shortcuts, navigation lock behavior, and performance-critical rendering are unchanged.
    - Test empty state, local slide, and remote slide flows for both small and large windows.
