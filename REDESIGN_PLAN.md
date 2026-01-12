# PathView UI Redesign — AI Coding Agent Prompt

## Role & Expectations

You are a **senior product engineer with strong design sensibility**, working in close collaboration with a **Stripe-level product designer**. Your task is to **redesign and refactor the UI/UX of a whole-slide image (WSI) viewer for digital pathology** called **PathView**.

This is not a cosmetic tweak. You are expected to **internalize the design critique below and translate it into concrete UI architecture, components, and interactions**. When making tradeoffs, prioritize **clarity, hierarchy, discoverability, and calm precision** over feature density.

The end result should feel like a **professional-grade scientific instrument**, not a developer tool.

---

## Product Intent

PathView is a high-performance viewer for multi-gigabyte Whole Slide Images (WSI) used in digital pathology. Users spend **hours** inside the app performing deep visual analysis.

The UI must:
- Inspire confidence and focus
- Be intuitive with minimal training
- Make power features discoverable
- Scale from novice to expert users

Think: **Figma × microscope × Stripe Dashboard**

---

## Core Design Principles (Non-Negotiable)

- **Hierarchy over density**: fewer visible controls, clearer importance
- **Mode-based interaction**: tools appear only when relevant
- **Contextual UI**: panels respond to what the user is doing
- **Designed empty states**: nothing should feel “missing”
- **Visual calm**: subtle depth, restrained contrast, deliberate spacing
- **Product language**: no developer shorthand or internal naming

---

## 1. Empty State & First Impression (Critical)

### Current Problem
- Vast empty dark canvas
- Floating “Welcome” modal feels temporary and apologetic
- Sidebar says “No slide loaded” (obvious, not helpful)

### Required Redesign
Implement a **full-bleed, intentional empty state**:

- Centered content, no modal card
- Large, confident headline:
  > *Explore whole-slide pathology at cellular resolution*
- Primary CTA (prominent):
  - **Open slide**
- Secondary CTA:
  - Connect to server
- Background:
  - Subtle texture or grid (microscopy-inspired)
  - Not flat black

This empty state should feel **designed**, not like a placeholder.

---

## 2. Top Bar: Introduce Interaction Modes

### Current Problem
- Flat list of buttons with equal weight
- No concept of user intent or workflow

### Required Structure

#### Left (App-Level)
- File
- View
- Help

#### Center (Interaction Mode — Segmented Control)
Implement **explicit modes**:
- Navigate
- Annotate
- Analyze

Only tools relevant to the active mode should be visible.

Example:
- Polygon Tool appears **only** in Annotate mode
- Measurement tools appear **only** in Analyze mode

#### Right (Contextual Controls)
- Reset View → icon-only
- Toggle Sidebar → icon-only

---

## 3. Sidebar: Replace Tabs with Contextual Sections

### Current Problem
- Tabbed interface with unclear purpose
- Feels like a dumping ground for features

### Required Redesign
Replace tabs with **stacked, contextual sections**.

---

### Section 1 — Slide Overview (Always Visible)
Purpose: grounding and orientation.

Include:
- Slide thumbnail
- Stain
- Magnification
- Dimensions
- Scanner / source

---

### Section 2 — Active Selection (Contextual)
Only appears when something is selected:
- ROI polygon
- Cell
- Annotation

Display:
- Area
- Cell counts
- Histomics metrics
- Actions:
  - Analyze
  - Export
  - Delete

This panel should dynamically update based on selection.

---

### Section 3 — Layers (Always Visible)
Think **Figma layers**, not ImageJ checkboxes.

Each layer includes:
- Visibility toggle (eye icon)
- Color chip
- Opacity slider

Examples:
- Slide
- Tumor regions
- Cell detections
- Heatmaps

---

## 4. Canvas: Visual Depth & Scale Awareness

### Current Problem
- Flat, lifeless canvas
- No spatial grounding

### Required Enhancements
- Always-visible scale bar (µm)

This is a **precision visual instrument** — it must feel physical.

---

## 5. Tool Discoverability & Onboarding

### Current Problem
- Power tools exist but are invisible to new users

### Required Features
- First-use, dismissible tooltips:
  - “Scroll to zoom”
  - “Click + drag to pan”
  - “Click to start polygon”
- Hover hints near cursor during annotation
- Subtle inline helper text (not documentation pages)

Goal: users should **accidentally discover power**.

---

## 6. Visual Language & Typography

### Required Standards
- Single refined sans-serif (Inter or SF Pro)
- Increased line-height everywhere
- Avoid pure black backgrounds
- Use one consistent accent color for:
  - Active tool
  - Selection
  - Focus state

Avoid:
- Harsh contrast
- Decorative gradients
- Visual noise

Think **calm, medical, trustworthy**.

---

## 7. Naming & Product Language

### Replace Developer Labels
- ❌ “Cell Poly…”
- ❌ “Polygon…”
- ❌ “Action ▶”

### With Product Language
- ✅ “Cell detections”
- ✅ “Annotations”
- ✅ “Export & Analyze”

Every label should be understandable without documentation.

---

## Implementation Expectations

- Refactor UI structure where needed (do not patch over bad structure)
- Introduce reusable components for:
  - Modes
  - Layers
  - Selection panels
- Keep performance critical paths untouched
- Maintain keyboard shortcuts and power-user flows

When in doubt, ask:
> “Would this feel obvious to a first-time pathologist?”

---

## Success Criteria

The redesign succeeds if:
- A new user understands what to do in <10 seconds
- Power features feel discoverable, not hidden
- The UI fades away during focused analysis
- The app feels credible as a commercial-grade pathology workstation

Deliver work that would pass a **Stripe design review**.
