#!/usr/bin/env python3
"""
Professional Presentation Generator for Zephyr & VL53L8CX Overview.
Transforms the existing PPTX into an executive-grade, modern, beautiful presentation.
"""

import sys
import os
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.enum.shapes import MSO_SHAPE
from pptx.dml.color import RGBColor

# -----------------------------------------------------------------------------
# Color Palette System
# -----------------------------------------------------------------------------
DARK_BG = RGBColor(11, 17, 32)       # Slate 950 (#0B1120)
DARK_CARD = RGBColor(20, 29, 48)     # Slate 900 (#141D30)
DARK_CARD_BORDER = RGBColor(38, 51, 78)

LIGHT_BG = RGBColor(248, 250, 252)   # Slate 50 (#F8FAFC)
WHITE = RGBColor(255, 255, 255)
BORDER_GRAY = RGBColor(226, 232, 240)# Slate 200 (#E2E8F0)
BORDER_STRONG = RGBColor(203, 213, 225)

PRIMARY_BLUE = RGBColor(37, 99, 235) # Blue 600 (#2563EB)
CYAN_ACCENT = RGBColor(6, 182, 212)  # Cyan 500 (#06B6D4)
INDIGO_ACCENT = RGBColor(99, 102, 241) # Indigo 500 (#6366F1)
PURPLE_ACCENT = RGBColor(139, 92, 246) # Purple 500

EMERALD_GREEN = RGBColor(16, 185, 129) # Green 500 (#10B981)
EMERALD_BG = RGBColor(236, 253, 245)  # Green 50
EMERALD_BORDER = RGBColor(167, 243, 208)

ROSE_RED = RGBColor(239, 68, 68)      # Red 500 (#EF4444)
ROSE_BG = RGBColor(254, 242, 242)     # Red 50
ROSE_BORDER = RGBColor(254, 202, 202)

AMBER_ORANGE = RGBColor(245, 158, 11) # Amber 500 (#F59E0B)
AMBER_BG = RGBColor(254, 243, 199)    # Amber 50
AMBER_BORDER = RGBColor(253, 230, 138)

BLUE_BG = RGBColor(238, 242, 255)     # Blue 50
BLUE_BORDER = RGBColor(199, 210, 254)

SLATE_BG = RGBColor(241, 245, 249)    # Slate 100
SLATE_BORDER = RGBColor(226, 232, 240)

TEXT_PRIMARY = RGBColor(15, 23, 42)    # Slate 900
TEXT_SECONDARY = RGBColor(71, 85, 105) # Slate 600
TEXT_MUTED = RGBColor(148, 163, 184)   # Slate 400
TEXT_LIGHT_MUTED = RGBColor(148, 163, 184)
TEXT_WHITE = RGBColor(255, 255, 255)

FONT_HEADING = "Segoe UI"
FONT_BODY = "Segoe UI"
FONT_CODE = "Consolas"

# -----------------------------------------------------------------------------
# Helper Functions
# -----------------------------------------------------------------------------
def set_shape_flat(shape, fill_color=None, line_color=None, line_width=Pt(1)):
    """Apply clean flat fill and border to any shape."""
    if fill_color:
        shape.fill.solid()
        shape.fill.fore_color.rgb = fill_color
    else:
        shape.fill.background()
    
    if line_color:
        shape.line.color.rgb = line_color
        shape.line.width = line_width
    else:
        shape.line.fill.background()

def add_header(slide, kicker: str, title: str, slide_num: int, total_slides: int = 15, kicker_color=PRIMARY_BLUE):
    """Adds a modern, polished slide header and footer."""
    # Top subtle category badge pill
    kicker_box = slide.shapes.add_textbox(Inches(0.7), Inches(0.4), Inches(8.0), Inches(0.35))
    tf_k = kicker_box.text_frame
    tf_k.word_wrap = True
    tf_k.margin_left = tf_k.margin_right = tf_k.margin_top = tf_k.margin_bottom = 0
    p_k = tf_k.paragraphs[0]
    p_k.text = kicker.upper()
    p_k.font.name = FONT_HEADING
    p_k.font.size = Pt(9.5)
    p_k.font.bold = True
    p_k.font.color.rgb = kicker_color

    # Slide Title
    title_box = slide.shapes.add_textbox(Inches(0.7), Inches(0.68), Inches(11.0), Inches(0.65))
    tf_t = title_box.text_frame
    tf_t.word_wrap = True
    tf_t.margin_left = tf_t.margin_right = tf_t.margin_top = tf_t.margin_bottom = 0
    p_t = tf_t.paragraphs[0]
    p_t.text = title
    p_t.font.name = FONT_HEADING
    p_t.font.size = Pt(22)
    p_t.font.bold = True
    p_t.font.color.rgb = TEXT_PRIMARY

    # Subtle accent underline bar
    accent_bar = slide.shapes.add_shape(
        MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.7), Inches(1.36), Inches(0.9), Inches(0.04)
    )
    set_shape_flat(accent_bar, fill_color=kicker_color, line_color=None)

    # Footer Left
    footer_left = slide.shapes.add_textbox(Inches(0.7), Inches(7.05), Inches(7.0), Inches(0.3))
    tf_fl = footer_left.text_frame
    tf_fl.margin_left = tf_fl.margin_right = tf_fl.margin_top = tf_fl.margin_bottom = 0
    p_fl = tf_fl.paragraphs[0]
    p_fl.text = "Zephyr Workspace & ST VL53L8CX Driver Architecture"
    p_fl.font.name = FONT_BODY
    p_fl.font.size = Pt(9.5)
    p_fl.font.color.rgb = TEXT_MUTED

    # Footer Right (Page Number)
    footer_right = slide.shapes.add_textbox(Inches(10.5), Inches(7.05), Inches(2.13), Inches(0.3))
    tf_fr = footer_right.text_frame
    tf_fr.margin_left = tf_fr.margin_right = tf_fr.margin_top = tf_fr.margin_bottom = 0
    p_fr = tf_fr.paragraphs[0]
    p_fr.alignment = PP_ALIGN.RIGHT
    p_fr.text = f"{slide_num:02d} / {total_slides:02d}"
    p_fr.font.name = FONT_BODY
    p_fr.font.size = Pt(9.5)
    p_fr.font.bold = True
    p_fr.font.color.rgb = TEXT_MUTED

def add_card(slide, left, top, width, height, bg_color=WHITE, border_color=BORDER_GRAY, corner_radius=None):
    """Creates a sleek container card."""
    card = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(left), Inches(top), Inches(width), Inches(height))
    set_shape_flat(card, fill_color=bg_color, line_color=border_color, line_width=Pt(1))
    return card

def add_bullet_point(tf, title: str, text: str, bullet_color=PRIMARY_BLUE, space_after=10):
    """Adds a styled bullet point with bold lead-in."""
    p = tf.add_paragraph() if len(tf.paragraphs[0].text) > 0 else tf.paragraphs[0]
    p.space_after = Pt(space_after)
    
    # Bullet symbol or lead-in
    r_bullet = p.add_run()
    r_bullet.text = "■  "
    r_bullet.font.name = FONT_BODY
    r_bullet.font.size = Pt(11)
    r_bullet.font.color.rgb = bullet_color
    
    if title:
        r_title = p.add_run()
        r_title.text = title + ": " if not title.endswith(":") and not title.endswith(".") else title + " "
        r_title.font.name = FONT_BODY
        r_title.font.size = Pt(12)
        r_title.font.bold = True
        r_title.font.color.rgb = TEXT_PRIMARY
    
    if text:
        r_text = p.add_run()
        r_text.text = text
        r_text.font.name = FONT_BODY
        r_text.font.size = Pt(12)
        r_text.font.bold = False
        r_text.font.color.rgb = TEXT_SECONDARY

# -----------------------------------------------------------------------------
# Slide Builders
# -----------------------------------------------------------------------------
def build_presentation(output_path: str):
    prs = Presentation()
    prs.slide_width = Inches(13.333)
    prs.slide_height = Inches(7.5)
    blank_layout = prs.slide_layouts[6]

    # =========================================================================
    # SLIDE 1: Title Slide (Dark Theme)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(13.333), Inches(7.5))
    set_shape_flat(bg, fill_color=DARK_BG, line_color=None)

    # Ambient Accent Glow card / banner
    glow_bar = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.9), Inches(1.8), Inches(11.533), Inches(4.3))
    set_shape_flat(glow_bar, fill_color=DARK_CARD, line_color=DARK_CARD_BORDER, line_width=Pt(1))

    # Top gradient-like accent line on card
    top_line = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.9), Inches(1.8), Inches(11.533), Inches(0.08))
    set_shape_flat(top_line, fill_color=CYAN_ACCENT, line_color=None)

    # Pill badge
    pill = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(1.4), Inches(2.25), Inches(3.2), Inches(0.38))
    set_shape_flat(pill, fill_color=RGBColor(30, 41, 69), line_color=INDIGO_ACCENT, line_width=Pt(1))
    p_tf = pill.text_frame
    p_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    p_p = p_tf.paragraphs[0]
    p_p.alignment = PP_ALIGN.CENTER
    p_p.text = "ZEPHYR RTOS  ·  WEST  ·  MODULES"
    p_p.font.name = FONT_HEADING
    p_p.font.size = Pt(9.5)
    p_p.font.bold = True
    p_p.font.color.rgb = CYAN_ACCENT

    # Title Text
    tb_title = slide.shapes.add_textbox(Inches(1.4), Inches(2.8), Inches(10.5), Inches(1.8))
    tf_title = tb_title.text_frame
    tf_title.word_wrap = True
    p_t = tf_title.paragraphs[0]
    p_t.text = "Zephyr Workspace Architecture\n& the VL53L8CX ToF Sensor Driver"
    p_t.font.name = FONT_HEADING
    p_t.font.size = Pt(34)
    p_t.font.bold = True
    p_t.font.color.rgb = TEXT_WHITE
    p_t.line_spacing = 1.15

    # Subtitle Text
    tb_sub = slide.shapes.add_textbox(Inches(1.4), Inches(4.55), Inches(10.5), Inches(1.0))
    tf_sub = tb_sub.text_frame
    tf_sub.word_wrap = True
    p_s = tf_sub.paragraphs[0]
    p_s.text = "How Zephyr, West, and external modules fit together — and where our STMicroelectronics VL53L8CX 8x8 multizone driver development stands today."
    p_s.font.name = FONT_BODY
    p_s.font.size = Pt(14)
    p_s.font.color.rgb = TEXT_LIGHT_MUTED

    # Footer Info
    tb_foot = slide.shapes.add_textbox(Inches(0.9), Inches(6.8), Inches(11.533), Inches(0.4))
    tf_foot = tb_foot.text_frame
    p_fl = tf_foot.paragraphs[0]
    p_fl.text = "zephyr_dev workspace overview"
    p_fl.font.name = FONT_BODY
    p_fl.font.size = Pt(10)
    p_fl.font.color.rgb = RGBColor(100, 116, 139)

    tb_foot_r = slide.shapes.add_textbox(Inches(9.0), Inches(6.8), Inches(3.433), Inches(0.4))
    tf_foot_r = tb_foot_r.text_frame
    p_fr = tf_foot_r.paragraphs[0]
    p_fr.alignment = PP_ALIGN.RIGHT
    p_fr.text = "September 2026"
    p_fr.font.name = FONT_BODY
    p_fr.font.size = Pt(10)
    p_fr.font.bold = True
    p_fr.font.color.rgb = RGBColor(100, 116, 139)


    # =========================================================================
    # SLIDE 2: Agenda (Light Theme, 6 Card Grid)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "Overview", "Agenda & Presentation Roadmap", 2)

    agenda_items = [
        ("01", "What is Zephyr RTOS?", "A scalable, open-source RTOS for modern embedded & IoT devices with hardware abstraction and rich subsystems."),
        ("02", "What is West?", "Zephyr's multi-repository meta-tool for orchestrating manifests, cloning dependencies, and building images."),
        ("03", "Zephyr's Folder Structure", "Deep dive into kernel/, arch/, drivers/, boards/, subsys/, and Kconfig organization."),
        ("04", "What is a Module?", "How external vendor SDKs and HALs plug seamlessly into the Zephyr build via module.yml."),
        ("05", "Our Workspace Layout", "Top-level architecture of zephyr_dev: zephyr_app, zephyr core, modules, and bootloader."),
        ("06", "VL53L8CX Driver Development", "Current status, glue vs. vendor architecture, licensing hygiene, and verified next steps."),
    ]

    card_w, card_h = 5.7, 1.55
    col_x = [0.7, 6.93]
    row_y = [1.65, 3.40, 5.15]

    for idx, (num, title, desc) in enumerate(agenda_items):
        cx = col_x[idx % 2]
        cy = row_y[idx // 2]
        
        # Card Container
        add_card(slide, cx, cy, card_w, card_h, bg_color=WHITE, border_color=BORDER_GRAY)
        
        # Left Accent Number Badge
        badge = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(cx + 0.2), Inches(cy + 0.25), Inches(0.65), Inches(0.65))
        set_shape_flat(badge, fill_color=BLUE_BG, line_color=BLUE_BORDER, line_width=Pt(1))
        b_tf = badge.text_frame
        b_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        b_p = b_tf.paragraphs[0]
        b_p.alignment = PP_ALIGN.CENTER
        b_p.text = num
        b_p.font.name = FONT_HEADING
        b_p.font.size = Pt(14)
        b_p.font.bold = True
        b_p.font.color.rgb = PRIMARY_BLUE

        # Title & Description
        tb = slide.shapes.add_textbox(Inches(cx + 1.05), Inches(cy + 0.2), Inches(card_w - 1.25), Inches(card_h - 0.35))
        tf = tb.text_frame
        tf.word_wrap = True
        tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
        
        p_t = tf.paragraphs[0]
        p_t.text = title
        p_t.font.name = FONT_HEADING
        p_t.font.size = Pt(13)
        p_t.font.bold = True
        p_t.font.color.rgb = TEXT_PRIMARY
        p_t.space_after = Pt(4)
        
        p_d = tf.add_paragraph()
        p_d.text = desc
        p_d.font.name = FONT_BODY
        p_d.font.size = Pt(10.5)
        p_d.font.color.rgb = TEXT_SECONDARY


    # =========================================================================
    # SLIDE 3: What is Zephyr? (Split Layout)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "Introduction", "What is Zephyr RTOS?", 3)

    # Left Card: Core Capabilities
    add_card(slide, 0.7, 1.65, 7.3, 5.15, bg_color=WHITE, border_color=BORDER_GRAY)
    
    tb_left = slide.shapes.add_textbox(Inches(0.95), Inches(1.85), Inches(6.8), Inches(4.75))
    tf_l = tb_left.text_frame
    tf_l.word_wrap = True
    tf_l.margin_left = tf_l.margin_right = tf_l.margin_top = tf_l.margin_bottom = 0

    add_bullet_point(tf_l, "Linux Foundation Governed", "Open-source, vendor-neutral RTOS with permissive Apache-2.0 licensing, backed by major industry leaders.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_l, "Massive Scalability", "Operates efficiently on ultra-low-power single-core MCUs up to high-performance multicore 64-bit MPUs.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_l, "Configuration-Driven", "Kconfig provides build-time modular selection, while Devicetree (DTS) completely decouples hardware topology from driver logic.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_l, "Broad Architecture Support", "Arm (Cortex-M/A/R), RISC-V, x86, Xtensa, and ARC across 600+ evaluated boards.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_l, "Production-Ready Subsystems", "Built-in Bluetooth LE, complete IPv4/IPv6 networking stack, USB host/device, flash filesystems, power management, and standardized sensor APIs.", PRIMARY_BLUE, 6)

    # Right Card: Spotlight / Why It Matters Here
    add_card(slide, 8.25, 1.65, 4.38, 5.15, bg_color=DARK_CARD, border_color=DARK_CARD_BORDER)
    
    # Spotlight Header Pill
    sp_pill = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(8.6), Inches(2.0), Inches(2.8), Inches(0.35))
    set_shape_flat(sp_pill, fill_color=RGBColor(30, 41, 69), line_color=CYAN_ACCENT, line_width=Pt(1))
    sp_tf = sp_pill.text_frame
    sp_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    sp_p = sp_tf.paragraphs[0]
    sp_p.alignment = PP_ALIGN.CENTER
    sp_p.text = "WHY IT MATTERS HERE"
    sp_p.font.name = FONT_HEADING
    sp_p.font.size = Pt(9.5)
    sp_p.font.bold = True
    sp_p.font.color.rgb = CYAN_ACCENT

    # Spotlight Content
    tb_right = slide.shapes.add_textbox(Inches(8.6), Inches(2.6), Inches(3.68), Inches(3.9))
    tf_r = tb_right.text_frame
    tf_r.word_wrap = True
    tf_r.margin_left = tf_r.margin_right = tf_r.margin_top = tf_r.margin_bottom = 0
    
    p_rh = tf_r.paragraphs[0]
    p_rh.text = "Target Portability"
    p_rh.font.name = FONT_HEADING
    p_rh.font.size = Pt(16)
    p_rh.font.bold = True
    p_rh.font.color.rgb = TEXT_WHITE
    p_rh.space_after = Pt(12)

    p_rb = tf_r.add_paragraph()
    p_rb.text = "Our STM32 hardware targets build on the exact same Zephyr RTOS foundation:\n\n• Nucleo-G474RE (Cortex-M4)\n• Nucleo-N657X0-Q (Cortex-M55 + NPU)\n• STM32MP257F-DK (Dual A35 + M33)\n\nThe build system, driver APIs (sensor_driver_api), and application code remain 100% identical; only the device tree overlay and vendor HAL change."
    p_rb.font.name = FONT_BODY
    p_rb.font.size = Pt(11)
    p_rb.font.color.rgb = TEXT_LIGHT_MUTED
    p_rb.line_spacing = 1.25


    # =========================================================================
    # SLIDE 4: What is West? (Split Layout)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "Introduction", "What is West? (Zephyr's Meta-Tool)", 4)

    # Left Card: West Concepts
    add_card(slide, 0.7, 1.65, 7.3, 5.15, bg_color=WHITE, border_color=BORDER_GRAY)
    
    tb_l4 = slide.shapes.add_textbox(Inches(0.95), Inches(1.85), Inches(6.8), Inches(4.75))
    tf_l4 = tb_l4.text_frame
    tf_l4.word_wrap = True
    tf_l4.margin_left = tf_l4.margin_right = tf_l4.margin_top = tf_l4.margin_bottom = 0

    add_bullet_point(tf_l4, "Multi-Repository Meta-Tool", "Zephyr's dedicated management CLI — solving multi-repo dependency orchestration cleaner than git submodules.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_l4, "Manifest-Driven Workflow", "Driven by west.yml: securely specifies all upstream repositories (Zephyr kernel, HALs, libraries) and their exact pinned git SHA revisions.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_l4, "Manifest Repository Concept", "The central entry-point repository that holds west.yml. Cloning this single repo allows West to reconstruct the entire multi-gigabyte ecosystem.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_l4, "Unified Development Commands", "Standardized commands across all OS platforms: west init, west update, west build, west flash, and west debug.", PRIMARY_BLUE, 6)

    # Right Card: Our Topology Spotlight
    add_card(slide, 8.25, 1.65, 4.38, 5.15, bg_color=DARK_CARD, border_color=DARK_CARD_BORDER)
    
    # Topology Pill
    top_pill = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(8.6), Inches(2.0), Inches(3.5), Inches(0.35))
    set_shape_flat(top_pill, fill_color=RGBColor(30, 41, 69), line_color=INDIGO_ACCENT, line_width=Pt(1))
    top_tf = top_pill.text_frame
    top_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    top_p = top_tf.paragraphs[0]
    top_p.alignment = PP_ALIGN.CENTER
    top_p.text = "OUR TOPOLOGY: APP-AS-MANIFEST (T2)"
    top_p.font.name = FONT_HEADING
    top_p.font.size = Pt(8.5)
    top_p.font.bold = True
    top_p.font.color.rgb = CYAN_ACCENT

    # Right Content
    tb_r4 = slide.shapes.add_textbox(Inches(8.6), Inches(2.55), Inches(3.68), Inches(4.0))
    tf_r4 = tb_r4.text_frame
    tf_r4.word_wrap = True
    tf_r4.margin_left = tf_r4.margin_right = tf_r4.margin_top = tf_r4.margin_bottom = 0
    
    p_r4h = tf_r4.paragraphs[0]
    p_r4h.text = "zephyr_app owns west.yml"
    p_r4h.font.name = FONT_HEADING
    p_r4h.font.size = Pt(15)
    p_r4h.font.bold = True
    p_r4h.font.color.rgb = TEXT_WHITE
    p_r4h.space_after = Pt(10)

    p_r4b = tf_r4.add_paragraph()
    p_r4b.text = "zephyr_app is our manifest repo (the only repository cloned initially).\n\nwest.yml directs west update to fetch siblings into the root:\n• zephyr/ (core RTOS kernel)\n• modules/hal/* (ST, CMSIS, etc.)\n• modules/lib/* (crypto, POSIX)\n• bootloader/mcuboot\n\nKeeps proprietary application code separated from upstream open-source code."
    p_r4b.font.name = FONT_BODY
    p_r4b.font.size = Pt(11)
    p_r4b.font.color.rgb = TEXT_LIGHT_MUTED
    p_r4b.line_spacing = 1.22


    # =========================================================================
    # SLIDE 5: Inside the zephyr/ Repository (3-Column Architecture Cards)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "Zephyr Internals", "Inside the zephyr/ Core Repository", 5)

    categories = [
        ("Core & Architecture", INDIGO_ACCENT, BLUE_BG, BLUE_BORDER, [
            ("kernel/", "Core OS: preemptive scheduler, thread management, memory slabs, mutexes, semaphores, and IPC queues."),
            ("arch/", "CPU architecture ports: ARM Cortex-M/A/R, RISC-V, x86, Xtensa, ARC, SPARC exception handling."),
            ("Kconfig", "Root build-time feature configuration tree (symbols, dependencies, menus)."),
        ]),
        ("Hardware & Drivers", PRIMARY_BLUE, SLATE_BG, BORDER_GRAY, [
            ("drivers/", "Hardware peripheral and sensor drivers indexed by class and vendor (e.g. sensor/st/vl53l1x)."),
            ("boards/", "Board definitions, default configurations, hardware devicetree overlays, and pin muxes."),
            ("dts/", "Devicetree bindings (.yaml) defining hardware properties and base SOC include files (.dtsi)."),
        ]),
        ("Subsystems & Testing", CYAN_ACCENT, SLATE_BG, BORDER_GRAY, [
            ("subsys/", "Higher-level stacks: Bluetooth/BLE host & controller, IP networking, USB, filesystems, logging."),
            ("samples/", "Ready-to-build example applications demonstrating peripheral & protocol usage."),
            ("tests/", "Comprehensive automated Twister test suite ensuring code quality across architectures."),
        ]),
    ]

    col_w = 3.75
    col_x_pos = [0.7, 4.79, 8.88]

    for c_idx, (cat_title, accent_color, bg_c, border_c, items) in enumerate(categories):
        x = col_x_pos[c_idx]
        
        # Column Card
        add_card(slide, x, 1.65, col_w, 5.15, bg_color=WHITE, border_color=BORDER_GRAY)
        
        # Header strip on card
        hdr_bar = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(x), Inches(1.65), Inches(col_w), Inches(0.55))
        set_shape_flat(hdr_bar, fill_color=bg_c, line_color=border_c, line_width=Pt(1))
        h_tf = hdr_bar.text_frame
        h_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        h_p = h_tf.paragraphs[0]
        h_p.alignment = PP_ALIGN.CENTER
        h_p.text = cat_title
        h_p.font.name = FONT_HEADING
        h_p.font.size = Pt(12)
        h_p.font.bold = True
        h_p.font.color.rgb = accent_color

        # Content items
        tb_col = slide.shapes.add_textbox(Inches(x + 0.2), Inches(2.35), Inches(col_w - 0.4), Inches(4.3))
        tf_c = tb_col.text_frame
        tf_c.word_wrap = True
        tf_c.margin_left = tf_c.margin_right = tf_c.margin_top = tf_c.margin_bottom = 0
        
        for item_idx, (folder_name, folder_desc) in enumerate(items):
            p_item = tf_c.add_paragraph() if item_idx > 0 else tf_c.paragraphs[0]
            p_item.space_after = Pt(12)
            
            # Badge folder name
            r_fn = p_item.add_run()
            r_fn.text = folder_name + "\n"
            r_fn.font.name = FONT_CODE
            r_fn.font.size = Pt(12)
            r_fn.font.bold = True
            r_fn.font.color.rgb = TEXT_PRIMARY
            
            # Description
            r_fd = p_item.add_run()
            r_fd.text = folder_desc
            r_fd.font.name = FONT_BODY
            r_fd.font.size = Pt(10.5)
            r_fd.font.color.rgb = TEXT_SECONDARY


    # =========================================================================
    # SLIDE 6: What is a “Module”? (Module Manifest & Categories)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "Zephyr Internals", "What is a Zephyr Module?", 6)

    # Left: Explanation Card
    add_card(slide, 0.7, 1.65, 7.3, 5.15, bg_color=WHITE, border_color=BORDER_GRAY)
    tb_m = slide.shapes.add_textbox(Inches(0.95), Inches(1.85), Inches(6.8), Inches(4.75))
    tf_m = tb_m.text_frame
    tf_m.word_wrap = True
    tf_m.margin_left = tf_m.margin_right = tf_m.margin_top = tf_m.margin_bottom = 0

    add_bullet_point(tf_m, "External Source Integration", "Modules allow external code (vendor HALs, 3rd-party stacks, crypto libraries, sensor SDKs) to seamlessly integrate into Zephyr's CMake & Kconfig build system.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_m, "Declared via zephyr/module.yml", "Every module contains a manifest file defining its module name, CMake hooks (cmake:, cmake-ext:), and Kconfig hooks (kconfig:, kconfig-ext:).", PRIMARY_BLUE, 14)
    add_bullet_point(tf_m, "West-Managed Modules", "Listed directly inside west.yml — automatically cloned, updated, and pinned to specific revisions during west update.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_m, "Out-of-Tree Modules (ZEPHYR_EXTRA_MODULES)", "Custom, in-development, or proprietary modules plugged into the build without needing an upstream west.yml entry.", PRIMARY_BLUE, 6)

    # Right Top: module.yml code card
    add_card(slide, 8.25, 1.65, 4.38, 2.7, bg_color=DARK_CARD, border_color=DARK_CARD_BORDER)
    
    tb_code_h = slide.shapes.add_textbox(Inches(8.5), Inches(1.8), Inches(3.8), Inches(0.3))
    tf_ch = tb_code_h.text_frame
    tf_ch.margin_left = tf_ch.margin_right = tf_ch.margin_top = tf_ch.margin_bottom = 0
    p_ch = tf_ch.paragraphs[0]
    p_ch.text = "TYPICAL zephyr/module.yml"
    p_ch.font.name = FONT_HEADING
    p_ch.font.size = Pt(10)
    p_ch.font.bold = True
    p_ch.font.color.rgb = CYAN_ACCENT

    tb_code = slide.shapes.add_textbox(Inches(8.5), Inches(2.2), Inches(3.8), Inches(2.0))
    tf_code = tb_code.text_frame
    tf_code.margin_left = tf_code.margin_right = tf_code.margin_top = tf_code.margin_bottom = 0
    p_c = tf_code.paragraphs[0]
    p_c.text = "name: hal_st\nbuild:\n  cmake-ext: True\n  kconfig-ext: True\n  settings:\n    dts_root: ."
    p_c.font.name = FONT_CODE
    p_c.font.size = Pt(11)
    p_c.font.color.rgb = TEXT_WHITE
    p_c.line_spacing = 1.3

    # Right Bottom: Categories Card
    add_card(slide, 8.25, 4.55, 4.38, 2.25, bg_color=BLUE_BG, border_color=BLUE_BORDER)
    
    tb_cat = slide.shapes.add_textbox(Inches(8.5), Inches(4.7), Inches(3.88), Inches(2.0))
    tf_cat = tb_cat.text_frame
    tf_cat.word_wrap = True
    tf_cat.margin_left = tf_cat.margin_right = tf_cat.margin_top = tf_cat.margin_bottom = 0
    
    p_ct = tf_cat.paragraphs[0]
    p_ct.text = "MODULE CATEGORIES IN OUR TREE"
    p_ct.font.name = FONT_HEADING
    p_ct.font.size = Pt(10)
    p_ct.font.bold = True
    p_ct.font.color.rgb = PRIMARY_BLUE
    p_ct.space_after = Pt(6)

    p_cb = tf_cat.add_paragraph()
    p_cb.text = "• hal/ (st, stm32, cmsis, nordic, nxp, ti)\n• lib/ (openthread, zcbor, littlefs, open-amp)\n• crypto/ (mbedtls, tf-psa-crypto)\n• sensor/ (st_vl53l8cx — our custom driver)"
    p_cb.font.name = FONT_BODY
    p_cb.font.size = Pt(10.5)
    p_cb.font.color.rgb = TEXT_PRIMARY
    p_cb.line_spacing = 1.25


    # =========================================================================
    # SLIDE 7: Top-Level Layout: zephyr_dev/ (Workspace Architecture)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "Our Workspace", "Top-Level Workspace Layout: zephyr_dev/", 7)

    # 4 Structured Component Cards
    components = [
        ("zephyr_app/", "MANIFEST REPOSITORY (Topdir)", "Owns west.yml. The only repository cloned directly by the engineer. Contains all target application code and board overlays.", INDIGO_ACCENT, BLUE_BG, BLUE_BORDER),
        ("zephyr/", "UPSTREAM CORE RTOS KERNEL", "Fetched by West from zephyrproject-rtos/zephyr. Contains scheduler, architecture ports, core subsystem APIs, and built-in drivers.", PRIMARY_BLUE, SLATE_BG, BORDER_GRAY),
        ("modules/", "VENDOR HALS & THIRD-PARTY LIBS", "Subdivided into hal/ (st, cmsis), lib/ (openthread), crypto/ (mbedtls), fs/. Each folder is an independent upstream git repository.", CYAN_ACCENT, SLATE_BG, BORDER_GRAY),
        ("modules/sensor/st_vl53l8cx", "OUR CUSTOM SENSOR DRIVER MODULE", "Our driver under development for ST's 8x8 multizone dToF sensor. Currently an out-of-tree module folder.", AMBER_ORANGE, AMBER_BG, AMBER_BORDER),
    ]

    for c_idx, (comp_name, comp_badge, comp_desc, acc_c, bg_c, bord_c) in enumerate(components):
        y = 1.65 + c_idx * 1.28
        
        # Container Card
        add_card(slide, 0.7, y, 11.933, 1.15, bg_color=WHITE, border_color=BORDER_GRAY)
        
        # Left Accent Chip
        add_card(slide, 0.7, y, 0.12, 1.15, bg_color=acc_c, border_color=None)
        
        # Component Title & Badge
        tb_comp = slide.shapes.add_textbox(Inches(1.05), Inches(y + 0.12), Inches(11.3), Inches(0.95))
        tf_cmp = tb_comp.text_frame
        tf_cmp.word_wrap = True
        tf_cmp.margin_left = tf_cmp.margin_right = tf_cmp.margin_top = tf_cmp.margin_bottom = 0
        
        p_c1 = tf_cmp.paragraphs[0]
        r_cn = p_c1.add_run()
        r_cn.text = comp_name + "    "
        r_cn.font.name = FONT_CODE
        r_cn.font.size = Pt(13)
        r_cn.font.bold = True
        r_cn.font.color.rgb = TEXT_PRIMARY
        
        r_cb = p_c1.add_run()
        r_cb.text = f"[{comp_badge}]"
        r_cb.font.name = FONT_HEADING
        r_cb.font.size = Pt(10)
        r_cb.font.bold = True
        r_cb.font.color.rgb = acc_c
        p_c1.space_after = Pt(3)

        p_c2 = tf_cmp.add_paragraph()
        p_c2.text = comp_desc
        p_c2.font.name = FONT_BODY
        p_c2.font.size = Pt(10.5)
        p_c2.font.color.rgb = TEXT_SECONDARY


    # =========================================================================
    # SLIDE 8: modules/ is a Folder, Not a Repo (Workspace Audit Table)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "Our Workspace", "modules/ is a Folder, Not a Repository", 8)

    # Left: Explanation Card
    add_card(slide, 0.7, 1.65, 6.6, 5.15, bg_color=WHITE, border_color=BORDER_GRAY)
    tb_m8 = slide.shapes.add_textbox(Inches(0.95), Inches(1.85), Inches(6.1), Inches(4.75))
    tf_m8 = tb_m8.text_frame
    tf_m8.word_wrap = True
    tf_m8.margin_left = tf_m8.margin_right = tf_m8.margin_top = tf_m8.margin_bottom = 0

    add_bullet_point(tf_m8, "Collection of Independent Repos", "modules/ itself is just a local filesystem directory. Every subdirectory inside it is an independent git repository cloned from GitHub.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_m8, "West Management Mechanics", "west update inspects west.yml and automatically clones ~50 individual repositories at exact pinned revisions into modules/hal/, modules/lib/, etc.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_m8, "The Hand-Placed Exception", "modules/sensor/st_vl53l8cx is NOT managed by West. It contains no .git folder and was placed manually in the workspace tree.", AMBER_ORANGE, 10)

    # Right: Verified Workspace Audit Grid
    add_card(slide, 7.55, 1.65, 5.08, 5.15, bg_color=WHITE, border_color=BORDER_GRAY)
    
    tb_aud_h = slide.shapes.add_textbox(Inches(7.85), Inches(1.85), Inches(4.5), Inches(0.4))
    tf_ah = tb_aud_h.text_frame
    tf_ah.margin_left = tf_ah.margin_right = tf_ah.margin_top = tf_ah.margin_bottom = 0
    p_ah = tf_ah.paragraphs[0]
    p_ah.text = "WORKSPACE AUDIT STATUS"
    p_ah.font.name = FONT_HEADING
    p_ah.font.size = Pt(11)
    p_ah.font.bold = True
    p_ah.font.color.rgb = PRIMARY_BLUE

    audit_rows = [
        ("modules/hal/st", "hal_st", "✓ Git Repo", EMERALD_GREEN, EMERALD_BG, EMERALD_BORDER),
        ("modules/hal/stm32", "hal_stm32", "✓ Git Repo", EMERALD_GREEN, EMERALD_BG, EMERALD_BORDER),
        ("modules/hal/cmsis", "cmsis", "✓ Git Repo", EMERALD_GREEN, EMERALD_BG, EMERALD_BORDER),
        ("modules/lib/openthread", "openthread", "✓ Git Repo", EMERALD_GREEN, EMERALD_BG, EMERALD_BORDER),
        ("modules/crypto/mbedtls", "mbedtls", "✓ Git Repo", EMERALD_GREEN, EMERALD_BG, EMERALD_BORDER),
        ("modules/sensor/st_vl53l8cx", "Custom Driver", "✗ No .git folder", ROSE_RED, ROSE_BG, ROSE_BORDER),
    ]

    for r_idx, (path_str, desc_str, stat_str, stat_c, stat_bg, stat_bord) in enumerate(audit_rows):
        ry = 2.35 + r_idx * 0.7
        
        # Row card
        row_c = add_card(slide, 7.85, ry, 4.48, 0.58, bg_color=SLATE_BG, border_color=BORDER_GRAY)
        
        # Text
        tb_r = slide.shapes.add_textbox(Inches(8.0), Inches(ry + 0.1), Inches(2.6), Inches(0.4))
        tf_r = tb_r.text_frame
        tf_r.margin_left = tf_r.margin_right = tf_r.margin_top = tf_r.margin_bottom = 0
        p_rp = tf_r.paragraphs[0]
        p_rp.text = path_str
        p_rp.font.name = FONT_CODE
        p_rp.font.size = Pt(9.5)
        p_rp.font.bold = True
        p_rp.font.color.rgb = TEXT_PRIMARY

        # Badge
        bdg = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(10.7), Inches(ry + 0.1), Inches(1.5), Inches(0.38))
        set_shape_flat(bdg, fill_color=stat_bg, line_color=stat_bord, line_width=Pt(1))
        b_tf = bdg.text_frame
        b_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        b_p = b_tf.paragraphs[0]
        b_p.alignment = PP_ALIGN.CENTER
        b_p.text = stat_str
        b_p.font.name = FONT_HEADING
        b_p.font.size = Pt(9.5)
        b_p.font.bold = True
        b_p.font.color.rgb = stat_c


    # =========================================================================
    # SLIDE 9: Section Divider (Dark Theme)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(13.333), Inches(7.5))
    set_shape_flat(bg, fill_color=DARK_BG, line_color=None)

    # Ambient Accent Glow card / banner
    glow_bar9 = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.9), Inches(1.8), Inches(11.533), Inches(4.3))
    set_shape_flat(glow_bar9, fill_color=DARK_CARD, line_color=DARK_CARD_BORDER, line_width=Pt(1))

    # Top gradient-like accent line on card
    top_line9 = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.9), Inches(1.8), Inches(11.533), Inches(0.08))
    set_shape_flat(top_line9, fill_color=INDIGO_ACCENT, line_color=None)

    # Pill badge
    pill9 = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(1.4), Inches(2.4), Inches(1.6), Inches(0.38))
    set_shape_flat(pill9, fill_color=RGBColor(30, 41, 69), line_color=INDIGO_ACCENT, line_width=Pt(1))
    p_tf9 = pill9.text_frame
    p_tf9.vertical_anchor = MSO_ANCHOR.MIDDLE
    p_p9 = p_tf9.paragraphs[0]
    p_p9.alignment = PP_ALIGN.CENTER
    p_p9.text = "PART TWO"
    p_p9.font.name = FONT_HEADING
    p_p9.font.size = Pt(9.5)
    p_p9.font.bold = True
    p_p9.font.color.rgb = CYAN_ACCENT

    # Title Text
    tb_title9 = slide.shapes.add_textbox(Inches(1.4), Inches(2.95), Inches(10.5), Inches(1.4))
    tf_title9 = tb_title9.text_frame
    tf_title9.word_wrap = True
    p_t9 = tf_title9.paragraphs[0]
    p_t9.text = "VL53L8CX Driver Development"
    p_t9.font.name = FONT_HEADING
    p_t9.font.size = Pt(32)
    p_t9.font.bold = True
    p_t9.font.color.rgb = TEXT_WHITE

    # Subtitle Text
    tb_sub9 = slide.shapes.add_textbox(Inches(1.4), Inches(4.5), Inches(10.5), Inches(1.0))
    tf_sub9 = tb_sub9.text_frame
    tf_sub9.word_wrap = True
    p_s9 = tf_sub9.paragraphs[0]
    p_s9.text = "Building a clean, production-grade Zephyr sensor driver for STMicroelectronics' 8x8 multizone direct Time-of-Flight (dToF) ranging sensor."
    p_s9.font.name = FONT_BODY
    p_s9.font.size = Pt(14)
    p_s9.font.color.rgb = TEXT_LIGHT_MUTED


    # =========================================================================
    # SLIDE 10: The Sensor & the Gap in Zephyr (Split Layout)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "VL53L8CX Driver", "The Sensor & The Ecosystem Gap in Zephyr", 10)

    # Left Card: Sensor Details & Landscape
    add_card(slide, 0.7, 1.65, 7.3, 5.15, bg_color=WHITE, border_color=BORDER_GRAY)
    tb_s10 = slide.shapes.add_textbox(Inches(0.95), Inches(1.85), Inches(6.8), Inches(4.75))
    tf_s10 = tb_s10.text_frame
    tf_s10.word_wrap = True
    tf_s10.margin_left = tf_s10.margin_right = tf_s10.margin_top = tf_s10.margin_bottom = 0

    add_bullet_point(tf_s10, "ST VL53L8CX 8x8 Multizone dToF", "Advanced optical time-of-flight sensor delivering up to 64 independent ranging zones across a wide 65° diagonal FOV with up to 4-meter range.", PRIMARY_BLUE, 14)
    add_bullet_point(tf_s10, "Existing Zephyr Single-Zone Drivers", "Zephyr currently contains drivers for older single-zone ST ToF sensors:\n• VL53L0X (drivers/sensor/st/vl53l0x — single zone, short range)\n• VL53L1X (drivers/sensor/st/vl53l1x — single zone, long range)", PRIMARY_BLUE, 14)
    add_bullet_point(tf_s10, "Zero Multizone Support in Zephyr", "Zephyr has NO existing driver anywhere in git history for the multizone dToF family (VL53L5CX, VL53L7CX, VL53L8CX, VL53L9CX).", ROSE_RED, 14)
    add_bullet_point(tf_s10, "No Open Upstream Pull Requests", "Confirmed: no community or vendor PR currently exists for this family in Zephyr.", PRIMARY_BLUE, 6)

    # Right Card: Real Gap, Real Value Spotlight
    add_card(slide, 8.25, 1.65, 4.38, 5.15, bg_color=DARK_CARD, border_color=DARK_CARD_BORDER)
    
    # Spotlight Pill
    v_pill = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(8.6), Inches(2.0), Inches(2.8), Inches(0.35))
    set_shape_flat(v_pill, fill_color=RGBColor(30, 41, 69), line_color=EMERALD_GREEN, line_width=Pt(1))
    v_tf = v_pill.text_frame
    v_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    v_p = v_tf.paragraphs[0]
    v_p.alignment = PP_ALIGN.CENTER
    v_p.text = "REAL GAP · REAL VALUE"
    v_p.font.name = FONT_HEADING
    v_p.font.size = Pt(9.5)
    v_p.font.bold = True
    v_p.font.color.rgb = EMERALD_GREEN

    # Spotlight Content
    tb_r10 = slide.shapes.add_textbox(Inches(8.6), Inches(2.6), Inches(3.68), Inches(3.9))
    tf_r10 = tb_r10.text_frame
    tf_r10.word_wrap = True
    tf_r10.margin_left = tf_r10.margin_right = tf_r10.margin_top = tf_r10.margin_bottom = 0
    
    p_10h = tf_r10.paragraphs[0]
    p_10h.text = "High Upstream Potential"
    p_10h.font.name = FONT_HEADING
    p_10h.font.size = Pt(16)
    p_10h.font.bold = True
    p_10h.font.color.rgb = TEXT_WHITE
    p_10h.space_after = Pt(12)

    p_10b = tf_r10.add_paragraph()
    p_10b.text = "Building this driver is not duplicating existing community efforts — the 8x8 multizone dToF family genuinely lacks Zephyr support today.\n\nA clean, robust, well-architected driver here delivers immediate project capability and holds high potential for upstream acceptance by the Zephyr sensor subsystem maintainers."
    p_10b.font.name = FONT_BODY
    p_10b.font.size = Pt(11)
    p_10b.font.color.rgb = TEXT_LIGHT_MUTED
    p_10b.line_spacing = 1.25


    # =========================================================================
    # SLIDE 11: Architecture: Glue Driver vs. Vendor Code (Visual Stack)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "VL53L8CX Driver", "Architecture: Glue Driver vs. Vendor Code", 11)

    # 5-Layer Stack
    layers = [
        ("Zephyr Application Layer", "Calls standard sensor_sample_fetch(dev) and sensor_channel_get(dev, ...)", PRIMARY_BLUE, BLUE_BG, BLUE_BORDER),
        ("Zephyr Sensor Subsystem  ·  vl53l8cx.c", "Implements sensor_driver_api, Devicetree binding resolution, and Kconfig integration", INDIGO_ACCENT, SLATE_BG, BORDER_GRAY),
        ("Hardware Glue Layer  ·  platform.c  [OUR CODE]", "Translates ST's abstract I/O calls to Zephyr i2c_write_read_dt(), GPIO controls & k_msleep()", AMBER_ORANGE, AMBER_BG, AMBER_BORDER),
        ("Vendor ULD Library  ·  uld/vl53l8cx_api.c  [PRISTINE ST CODE]", "ST's raw ranging algorithms, firmware loader, and register maps — 100% agnostic of Zephyr", CYAN_ACCENT, SLATE_BG, BORDER_GRAY),
        ("Physical Hardware Layer  ·  I2C / SPI & Interrupts", "ST VL53L8CX sensor, I2C/SPI bus lines, LPn power pins, and INT GPIO interrupts", TEXT_MUTED, DARK_CARD, DARK_CARD_BORDER),
    ]

    for l_idx, (l_title, l_desc, acc_c, bg_c, bord_c) in enumerate(layers):
        ly = 1.65 + l_idx * 1.0
        
        # Container Card
        text_color = TEXT_WHITE if bg_c == DARK_CARD else TEXT_PRIMARY
        desc_color = TEXT_LIGHT_MUTED if bg_c == DARK_CARD else TEXT_SECONDARY
        
        add_card(slide, 0.7, ly, 7.8, 0.9, bg_color=bg_c, border_color=bord_c)
        add_card(slide, 0.7, ly, 0.1, 0.9, bg_color=acc_c, border_color=None)
        
        tb_lay = slide.shapes.add_textbox(Inches(0.95), Inches(ly + 0.08), Inches(7.4), Inches(0.75))
        tf_lay = tb_lay.text_frame
        tf_lay.word_wrap = True
        tf_lay.margin_left = tf_lay.margin_right = tf_lay.margin_top = tf_lay.margin_bottom = 0
        
        p_lh = tf_lay.paragraphs[0]
        p_lh.text = l_title
        p_lh.font.name = FONT_HEADING
        p_lh.font.size = Pt(11)
        p_lh.font.bold = True
        p_lh.font.color.rgb = text_color
        p_lh.space_after = Pt(2)

        p_ld = tf_lay.add_paragraph()
        p_ld.text = l_desc
        p_ld.font.name = FONT_BODY
        p_ld.font.size = Pt(9.5)
        p_ld.font.color.rgb = desc_color

    # Right Card: Architectural Principle
    add_card(slide, 8.8, 1.65, 3.833, 4.9, bg_color=DARK_CARD, border_color=DARK_CARD_BORDER)
    
    # Principle Pill
    pr_pill = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(9.1), Inches(2.0), Inches(2.2), Inches(0.35))
    set_shape_flat(pr_pill, fill_color=RGBColor(30, 41, 69), line_color=CYAN_ACCENT, line_width=Pt(1))
    pr_tf = pr_pill.text_frame
    pr_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    pr_p = pr_tf.paragraphs[0]
    pr_p.alignment = PP_ALIGN.CENTER
    pr_p.text = "CORE PRINCIPLE"
    pr_p.font.name = FONT_HEADING
    pr_p.font.size = Pt(9.5)
    pr_p.font.bold = True
    pr_p.font.color.rgb = CYAN_ACCENT

    tb_princ = slide.shapes.add_textbox(Inches(9.1), Inches(2.6), Inches(3.233), Inches(3.7))
    tf_princ = tb_princ.text_frame
    tf_princ.word_wrap = True
    tf_princ.margin_left = tf_princ.margin_right = tf_princ.margin_top = tf_princ.margin_bottom = 0
    
    p_ph = tf_princ.paragraphs[0]
    p_ph.text = "Strict Separation of Concerns"
    p_ph.font.name = FONT_HEADING
    p_ph.font.size = Pt(15)
    p_ph.font.bold = True
    p_ph.font.color.rgb = TEXT_WHITE
    p_ph.space_after = Pt(10)

    p_pb = tf_princ.add_paragraph()
    p_pb.text = "1. Glue Layer (vl53l8cx.c & platform.c)\nOurs to write, optimize, and maintain freely under Apache-2.0.\n\n2. Vendor ULD (uld/vl53l8cx_api.c)\nMust remain 100% pristine ST licensed code (BSD-3-Clause). Zero Zephyr-specific #includes or printk() statements allowed inside vendor source."
    p_pb.font.name = FONT_BODY
    p_pb.font.size = Pt(10.5)
    p_pb.font.color.rgb = TEXT_LIGHT_MUTED
    p_pb.line_spacing = 1.25


    # =========================================================================
    # SLIDE 12: The Existing Precedent: VL53L1X (Side-by-Side Blueprint)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "VL53L8CX Driver", "The Existing Upstream Precedent: VL53L1X", 12)

    # Subtitle intro box
    tb_intro = slide.shapes.add_textbox(Inches(0.7), Inches(1.5), Inches(11.933), Inches(0.4))
    tf_in = tb_intro.text_frame
    tf_in.margin_left = tf_in.margin_right = tf_in.margin_top = tf_in.margin_bottom = 0
    p_in = tf_in.paragraphs[0]
    p_in.text = "Zephyr never mixes vendor SDK code directly into its kernel tree — it cleanly splits drivers across two repositories:"
    p_in.font.name = FONT_BODY
    p_in.font.size = Pt(11.5)
    p_in.font.bold = True
    p_in.font.color.rgb = TEXT_PRIMARY

    # Card 1: Zephyr Tree Glue
    add_card(slide, 0.7, 2.0, 5.8, 3.8, bg_color=WHITE, border_color=BORDER_GRAY)
    add_card(slide, 0.7, 2.0, 5.8, 0.5, bg_color=BLUE_BG, border_color=BLUE_BORDER)
    
    tb_c1h = slide.shapes.add_textbox(Inches(0.9), Inches(2.1), Inches(5.4), Inches(0.35))
    tf_c1h = tb_c1h.text_frame
    p_c1h = tf_c1h.paragraphs[0]
    p_c1h.text = "zephyr/drivers/sensor/st/vl53l1x/  (Zephyr Tree)"
    p_c1h.font.name = FONT_CODE
    p_c1h.font.size = Pt(11)
    p_c1h.font.bold = True
    p_c1h.font.color.rgb = PRIMARY_BLUE

    tb_c1b = slide.shapes.add_textbox(Inches(0.95), Inches(2.65), Inches(5.3), Inches(3.0))
    tf_c1b = tb_c1b.text_frame
    tf_c1b.word_wrap = True
    p_c1b = tf_c1b.paragraphs[0]
    p_c1b.text = "• vl53l1.c  — Zephyr sensor API glue implementation\n• vl53l1_platform.c  — I2C & GPIO bus wrappers\n• Kconfig & CMakeLists.txt  — driver configuration\n\nLicense: Apache-2.0 (standard Zephyr permissive)\nOwned & Reviewed by: Zephyr sensor subsystem maintainers"
    p_c1b.font.name = FONT_BODY
    p_c1b.font.size = Pt(11)
    p_c1b.font.color.rgb = TEXT_SECONDARY
    p_c1b.line_spacing = 1.3

    # Card 2: HAL ST Vendor ULD
    add_card(slide, 6.833, 2.0, 5.8, 3.8, bg_color=WHITE, border_color=BORDER_GRAY)
    add_card(slide, 6.833, 2.0, 5.8, 0.5, bg_color=SLATE_BG, border_color=BORDER_GRAY)
    
    tb_c2h = slide.shapes.add_textbox(Inches(7.033), Inches(2.1), Inches(5.4), Inches(0.35))
    tf_c2h = tb_c2h.text_frame
    p_c2h = tf_c2h.paragraphs[0]
    p_c2h.text = "modules/hal/st/  (hal_st Module Repo)"
    p_c2h.font.name = FONT_CODE
    p_c2h.font.size = Pt(11)
    p_c2h.font.bold = True
    p_c2h.font.color.rgb = TEXT_PRIMARY

    tb_c2b = slide.shapes.add_textbox(Inches(7.083), Inches(2.65), Inches(5.3), Inches(3.0))
    tf_c2b = tb_c2b.text_frame
    tf_c2b.word_wrap = True
    p_c2b = tf_c2b.paragraphs[0]
    p_c2b.text = "• sensor/vl53l1x/api/core/src/vl53l1_api.c\n• vl53l1_api_calibration.c, vl53l1_api_core.c, ...\n• STMicroelectronics raw sensor algorithms & register maps\n\nLicense: BSD-3-Clause / GPL-2.0+ (ST official license)\nOwned & Reviewed by: STMicroelectronics & hal_st maintainers"
    p_c2b.font.name = FONT_BODY
    p_c2b.font.size = Pt(11)
    p_c2b.font.color.rgb = TEXT_SECONDARY
    p_c2b.line_spacing = 1.3

    # Bottom Banner: Target Blueprint
    add_card(slide, 0.7, 5.95, 11.933, 0.85, bg_color=DARK_CARD, border_color=DARK_CARD_BORDER)
    add_card(slide, 0.7, 5.95, 0.1, 0.85, bg_color=CYAN_ACCENT, border_color=None)
    
    tb_bot = slide.shapes.add_textbox(Inches(1.0), Inches(6.05), Inches(11.4), Inches(0.65))
    tf_bot = tb_bot.text_frame
    tf_bot.word_wrap = True
    p_bot = tf_bot.paragraphs[0]
    p_bot.text = "Target Blueprint for VL53L8CX: Our vl53l8cx.c and platform.c will serve as the upstream Zephyr sensor glue, while our uld/ folder will be submitted upstream to modules/hal/st."
    p_bot.font.name = FONT_BODY
    p_bot.font.size = Pt(11)
    p_bot.font.bold = True
    p_bot.font.color.rgb = CYAN_ACCENT


    # =========================================================================
    # SLIDE 13: What We Verified So Far (Confirmed vs. Blocking)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "VL53L8CX Driver", "Driver Audit: Confirmed Findings vs. Current Blockers", 13)

    # Left Column: Confirmed (Green Theme)
    add_card(slide, 0.7, 1.65, 5.8, 5.15, bg_color=WHITE, border_color=EMERALD_BORDER)
    
    # Header Banner Green
    add_card(slide, 0.7, 1.65, 5.8, 0.55, bg_color=EMERALD_BG, border_color=EMERALD_BORDER)
    tb_ch = slide.shapes.add_textbox(Inches(0.95), Inches(1.75), Inches(5.3), Inches(0.35))
    tf_ch = tb_ch.text_frame
    p_ch = tf_ch.paragraphs[0]
    p_ch.text = "CONFIRMED & READY"
    p_ch.font.name = FONT_HEADING
    p_ch.font.size = Pt(12)
    p_ch.font.bold = True
    p_ch.font.color.rgb = EMERALD_GREEN

    confirmed_items = [
        ("Genuine ST ULD Source Code", "Vendor uld/*.c source diffed against official stm32duino/VL53L8CX repository — verified genuine and algorithmically identical."),
        ("License Compatibility", "ST's ULD driver family is BSD-3-Clause licensed — 100% compatible with Zephyr Project licensing requirements."),
        ("High-Value Ecosystem Gap", "Fills a true void: no VL53L5/7/8/9CX multizone driver exists anywhere in Zephyr today."),
    ]

    for idx, (head, desc) in enumerate(confirmed_items):
        y = 2.35 + idx * 1.45
        
        # Check badge
        bdg = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.95), Inches(y), Inches(0.4), Inches(0.4))
        set_shape_flat(bdg, fill_color=EMERALD_GREEN, line_color=None)
        b_tf = bdg.text_frame
        b_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        b_p = b_tf.paragraphs[0]
        b_p.alignment = PP_ALIGN.CENTER
        b_p.text = "✓"
        b_p.font.name = FONT_HEADING
        b_p.font.size = Pt(14)
        b_p.font.bold = True
        b_p.font.color.rgb = WHITE

        tb_i = slide.shapes.add_textbox(Inches(1.45), Inches(y - 0.05), Inches(4.85), Inches(1.3))
        tf_i = tb_i.text_frame
        tf_i.word_wrap = True
        p_ih = tf_i.paragraphs[0]
        p_ih.text = head
        p_ih.font.name = FONT_HEADING
        p_ih.font.size = Pt(11.5)
        p_ih.font.bold = True
        p_ih.font.color.rgb = TEXT_PRIMARY
        p_ih.space_after = Pt(2)

        p_id = tf_i.add_paragraph()
        p_id.text = desc
        p_id.font.name = FONT_BODY
        p_id.font.size = Pt(10)
        p_id.font.color.rgb = TEXT_SECONDARY

    # Right Column: Still Blocking (Red Theme)
    add_card(slide, 6.833, 1.65, 5.8, 5.15, bg_color=WHITE, border_color=ROSE_BORDER)
    
    # Header Banner Red
    add_card(slide, 6.833, 1.65, 5.8, 0.55, bg_color=ROSE_BG, border_color=ROSE_BORDER)
    tb_bh = slide.shapes.add_textbox(Inches(7.083), Inches(1.75), Inches(5.3), Inches(0.35))
    tf_bh = tb_bh.text_frame
    p_bh = tf_bh.paragraphs[0]
    p_bh.text = "CURRENT BLOCKERS & DEFECTS"
    p_bh.font.name = FONT_HEADING
    p_bh.font.size = Pt(12)
    p_bh.font.bold = True
    p_bh.font.color.rgb = ROSE_RED

    blocking_items = [
        ("Empty Build Files", "module.yml, Kconfig, and CMakeLists.txt are empty stubs — the module does not currently build in Zephyr."),
        ("Polluted Vendor Source", "uld/vl53l8cx_api.c contains debug printk() calls and a Zephyr #include hand-spliced into vendor code."),
        ("Missing License File & Attribution", "No LICENSE file shipped alongside vendor code; header uses placeholder 'Zephyr Community'."),
        ("No Version Control History", "Not under git control — no repository, no commit history, and no attribution trail."),
    ]

    for idx, (head, desc) in enumerate(blocking_items):
        y = 2.3 + idx * 1.12
        
        # Cross badge
        bdg = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(7.083), Inches(y), Inches(0.35), Inches(0.35))
        set_shape_flat(bdg, fill_color=ROSE_RED, line_color=None)
        b_tf = bdg.text_frame
        b_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        b_p = b_tf.paragraphs[0]
        b_p.alignment = PP_ALIGN.CENTER
        b_p.text = "✗"
        b_p.font.name = FONT_HEADING
        b_p.font.size = Pt(12)
        b_p.font.bold = True
        b_p.font.color.rgb = WHITE

        tb_i = slide.shapes.add_textbox(Inches(7.55), Inches(y - 0.05), Inches(4.88), Inches(1.0))
        tf_i = tb_i.text_frame
        tf_i.word_wrap = True
        p_ih = tf_i.paragraphs[0]
        p_ih.text = head
        p_ih.font.name = FONT_HEADING
        p_ih.font.size = Pt(11)
        p_ih.font.bold = True
        p_ih.font.color.rgb = TEXT_PRIMARY
        p_ih.space_after = Pt(2)

        p_id = tf_i.add_paragraph()
        p_id.text = desc
        p_id.font.name = FONT_BODY
        p_id.font.size = Pt(9.5)
        p_id.font.color.rgb = TEXT_SECONDARY


    # =========================================================================
    # SLIDE 14: Next Steps (5-Step Numbered Execution Roadmap)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    add_header(slide, "VL53L8CX Driver", "Execution Roadmap: Next Steps to Production", 14)

    steps = [
        ("01", "Clean the Vendor Source File", "Restore uld/vl53l8cx_api.c to a 100% pristine ST release copy. Relocate all debug diagnostics into platform.c and vl53l8cx.c using standard Zephyr LOG_DBG().", PRIMARY_BLUE),
        ("02", "Make the Driver Module Buildable", "Populate zephyr/module.yml, Kconfig, CMakeLists.txt (root + driver levels), and complete the formal devicetree binding YAML (st,vl53l8cx.yaml).", INDIGO_ACCENT),
        ("03", "Fix Licensing & Copyright Hygiene", "Attach ST's official BSD-3-Clause LICENSE file to uld/ and replace placeholder copyright headers with accurate author & license attributions.", CYAN_ACCENT),
        ("04", "Validate on Physical Target Hardware", "Build, flash, and execute across our hardware test matrix (Nucleo-G474RE, Nucleo-N657X0-Q, STM32MP257F) with automated I2C ranging checks.", EMERALD_GREEN),
        ("05", "Decide on Long-Term Upstream Architecture", "Maintain as an unified out-of-tree module for rapid internal iteration; split into Zephyr tree glue + hal_st contribution when submitting upstream.", AMBER_ORANGE),
    ]

    for s_idx, (num, s_title, s_desc, s_color) in enumerate(steps):
        sy = 1.65 + s_idx * 1.02
        
        # Step Container Card
        add_card(slide, 0.7, sy, 11.933, 0.92, bg_color=WHITE, border_color=BORDER_GRAY)
        
        # Step Number Badge
        num_b = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.85), Inches(sy + 0.16), Inches(0.6), Inches(0.6))
        set_shape_flat(num_b, fill_color=s_color, line_color=None)
        nb_tf = num_b.text_frame
        nb_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        nb_p = nb_tf.paragraphs[0]
        nb_p.alignment = PP_ALIGN.CENTER
        nb_p.text = num
        nb_p.font.name = FONT_HEADING
        nb_p.font.size = Pt(13)
        nb_p.font.bold = True
        nb_p.font.color.rgb = WHITE

        # Step Text
        tb_st = slide.shapes.add_textbox(Inches(1.6), Inches(sy + 0.1), Inches(10.8), Inches(0.75))
        tf_st = tb_st.text_frame
        tf_st.word_wrap = True
        tf_st.margin_left = tf_st.margin_right = tf_st.margin_top = tf_st.margin_bottom = 0
        
        p_sth = tf_st.paragraphs[0]
        p_sth.text = s_title
        p_sth.font.name = FONT_HEADING
        p_sth.font.size = Pt(11.5)
        p_sth.font.bold = True
        p_sth.font.color.rgb = TEXT_PRIMARY
        p_sth.space_after = Pt(2)

        p_std = tf_st.add_paragraph()
        p_std.text = s_desc
        p_std.font.name = FONT_BODY
        p_std.font.size = Pt(10)
        p_std.font.color.rgb = TEXT_SECONDARY


    # =========================================================================
    # SLIDE 15: Thank You (Dark Theme)
    # =========================================================================
    slide = prs.slides.add_slide(blank_layout)
    bg = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(13.333), Inches(7.5))
    set_shape_flat(bg, fill_color=DARK_BG, line_color=None)

    # Ambient Accent Glow card / banner
    glow_bar15 = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.9), Inches(1.8), Inches(11.533), Inches(4.3))
    set_shape_flat(glow_bar15, fill_color=DARK_CARD, line_color=DARK_CARD_BORDER, line_width=Pt(1))

    # Top gradient-like accent line on card
    top_line15 = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(0.9), Inches(1.8), Inches(11.533), Inches(0.08))
    set_shape_flat(top_line15, fill_color=EMERALD_GREEN, line_color=None)

    # Title
    tb_ty = slide.shapes.add_textbox(Inches(1.4), Inches(2.6), Inches(10.5), Inches(1.2))
    tf_ty = tb_ty.text_frame
    p_ty = tf_ty.paragraphs[0]
    p_ty.text = "Thank You"
    p_ty.font.name = FONT_HEADING
    p_ty.font.size = Pt(40)
    p_ty.font.bold = True
    p_ty.font.color.rgb = TEXT_WHITE

    # Subtitle
    tb_tys = slide.shapes.add_textbox(Inches(1.4), Inches(3.9), Inches(10.5), Inches(0.8))
    tf_tys = tb_tys.text_frame
    p_tys = tf_tys.paragraphs[0]
    p_tys.text = "Open for Questions, Technical Discussion & Hardware Validation"
    p_tys.font.name = FONT_BODY
    p_tys.font.size = Pt(16)
    p_tys.font.color.rgb = CYAN_ACCENT

    # Path badge card
    path_card = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Inches(1.4), Inches(4.8), Inches(7.0), Inches(0.5))
    set_shape_flat(path_card, fill_color=RGBColor(30, 41, 69), line_color=DARK_CARD_BORDER, line_width=Pt(1))
    pc_tf = path_card.text_frame
    pc_tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    pc_p = pc_tf.paragraphs[0]
    pc_p.text = "Workspace Location: zephyr_dev/modules/sensor/st_vl53l8cx"
    pc_p.font.name = FONT_CODE
    pc_p.font.size = Pt(11)
    pc_p.font.bold = True
    pc_p.font.color.rgb = TEXT_LIGHT_MUTED

    # Save presentation
    prs.save(output_path)
    print(f"Successfully generated {output_path}")

if __name__ == "__main__":
    out_file = sys.argv[1] if len(sys.argv) > 1 else "documentation/Zephyr_West_VL53L8CX_Overview.pptx"
    build_presentation(out_file)

