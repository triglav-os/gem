/*
 * Declares the shared manual-demo runner used by samples/demo3 and up.
 * Each scene reproduces one focused VDI example derived from the
 * Programmer's Guide so the demos can be launched independently.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef SAMPLES_MANUAL_MANUAL_DEMO_H
#define SAMPLES_MANUAL_MANUAL_DEMO_H

#include <gem/portab.h>

typedef enum manual_demo_scene {
    manual_demo_scene_listing_2_1 = 0,
    manual_demo_scene_clipping,
    manual_demo_scene_polyline_and_markers,
    manual_demo_scene_text_and_fonts,
    manual_demo_scene_fillarea_and_bar,
    manual_demo_scene_cellarray,
    manual_demo_scene_contourfill,
    manual_demo_scene_arc_family,
    manual_demo_scene_rounded_boxes,
    manual_demo_scene_justified,
    manual_demo_scene_write_modes,
    manual_demo_scene_blits,
    manual_demo_scene_queries,
    manual_demo_scene_cursor_helpers
} manual_demo_scene_t;

/*
 * Opens a VDI workstation, draws the selected manual scene, and keeps the
 * result on screen until the process is interrupted.
 * Returns 0 on success or 1 if the workstation could not be opened.
 */
int manual_demo_run(manual_demo_scene_t scene);

#endif
