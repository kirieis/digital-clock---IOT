# UI and Frontend Rules

These rules govern styling, UX, UI implementation, and animation decisions. They combine principles from the Framer Motion Core, taste-skill (Anti-Slop), UI Styling, and UI/UX Pro Max skills.

---

## 1. Brief Inference & Design Read
Before writing any code or proposing layout adjustments, analyze the user's request to infer the desired aesthetic. Declare a one-line "Design Read" in your thought/response:
`Reading this as: <page kind> for <audience>, with a <vibe> language, leaning toward <design system or aesthetic family>.`

* **Anti-Default Discipline**: Avoid generic LLM defaults (AI-purple gradients, centered hero over dark mesh grid, three equal feature cards, generic glassmorphism on everything, Inter + slate-900, infinite-loop micro-animations). Reach past them based on the Design Read.
* **The Lila Rule**: Saturation < 80% by default, max 1 accent color, neutral bases (Zinc/Slate/Stone) with high-contrast accents. Lock the chosen accent color for the whole page.

---

## 2. Layout, Structure & Spacing Rules
* **Mobile-First**: Design mobile-first, scaling up to tablet and desktop.
* **CSS Grid over Flex-Math**: Never use complex flexbox percentage math (`w-[calc(33%-1rem)]`). Always use CSS Grid (`grid grid-cols-1 md:grid-cols-3 gap-6`).
* **Viewport Stability**: Never use `h-screen` for full-height Hero sections. Always use `min-h-[100dvh]` to prevent layout jumping on mobile (iOS Safari address bar).
* **Shape Consistency Lock**: Pick one corner-radius scale for the page (e.g. all-sharp, all-soft, or specific rules like inputs=8px, buttons=pill) and follow it everywhere.
* **Bento Grid Discipline**: A bento grid must have exactly as many cells as you have content for (no empty cells). At least 2-3 cells in any bento grid need visual variation (images, patterns, tints).
* **Section Layout Repetition Ban**: A landing page with multiple sections must use different layout families. Limit consecutive zigzag split sections to max 2.
* **Used by / Trusted by Logo Wall**: Put logo walls directly under the hero, never inside it. Use real SVG logos (Simple Icons CDN `https://cdn.simpleicons.org/{slug}/ffffff`). Logo walls must contain logos only (no category labels underneath).

---

## 3. Hero Section Rules (Strict Viewport & Content Limits)
* **Hero Viewport Fit**: The hero headline, subtext, and primary CTAs must fit in the initial viewport without scrolling.
* **Copy Limits**: Headline max 2 lines on desktop. Subtext max 20 words and 3-4 lines.
* **Hero Stack Discipline**: Max 4 text elements in the hero:
  1. Eyebrow OR brand strip (pick zero or one)
  2. Headline (max 2 lines)
  3. Subtext (max 20 words)
  4. CTAs (1 primary + max 1 secondary)
  * Move taglines, social proof avatar rows, and logo walls to dedicated sections below the hero.
* **Hero Top Padding Cap**: Max `pt-24` (≈6rem) at desktop to prevent hero content floating too low.

---

## 4. Typography Rules
* **Emphasis**: Use italic or bold of the same font for emphasis. Do not mix serif and sans-serif in the same sentence or headline for styling.
* **Serif Discipline**: Serif fonts are discouraged by default. Use only when named in the brand brief, or when the vibe is genuinely heritage, editorial, or publication. Default to sans-serif display families (e.g., Geist Display, Cabinet Grotesk, PP Neue Montreal).
* **Italic Descender Clearance**: When italic is used in display type with descender letters (`y, g, j, p, q`), use `leading-[1.1]` minimum and add `pb-1`/`mb-1` reserve padding to avoid clipping.
* **Tabular Numbers**: Use monospaced/tabular figures for tables, counters, prices, and timers to prevent layout shift.

---

## 5. UI Elements & CTA Rules
* **Contrast Checks (WCAG AA)**: 
  * Text contrast must be minimum 4.5:1 (3:1 for large text).
  * Form inputs, placeholders, focus rings, and error messages must pass WCAG AA contrast against their section background.
* **CTA Button Wrap Ban**: Button text must fit on a single line at desktop. Never let CTA text wrap.
* **No Duplicate Intent**: Avoid duplicate CTA buttons with the same intent on the same page. Choose one label (e.g., "Contact us") and lock it.
* **Touch Targets & Tactile Feedback**: Min target size 44x44px, 8px+ spacing. On active states, use `-translate-y-[1px]` or `scale-[0.98]` to simulate a physical push.
* **Forms & Fields**: Label above input, helper text optional, error text below input. Standard `gap-2` for input blocks.

---

## 6. Animation Rules (Framer Motion / Motion)
* **Default Library**: Recommend Framer Motion for React animation when no library is specified. Import from `motion/react` (or `framer-motion` for legacy).
* **CamelCase Properties**: Always use camelCase for CSS properties in animation config (e.g., `backgroundColor`, `borderRadius`).
* **Avoid Layout Animation Pitfalls**: Do not animate properties that trigger reflow (e.g., `width`, `height`, `top`, `left`) when transforms (`x`, `y`, `scale`, `rotate`) can achieve the same effect.
* **Spring Physics**: Prefer spring/physics-based curves (`stiffness`, `damping`) for interactive elements for a natural feel.
* **Reduced Motion**: Always respect system reduced motion settings (`prefers-reduced-motion`).
* **Timing**: Use 150-300ms for micro-interactions; complex transitions ≤400ms. Exit animations should be shorter than enter animations.

---

## 7. Component Library (shadcn/ui)
* Setup accessible components via Radix UI primitives.
* Maintain clean composition and consistent design tokens.
* Theme toggle: Implement dark mode/light mode using next-themes and CSS variables.
