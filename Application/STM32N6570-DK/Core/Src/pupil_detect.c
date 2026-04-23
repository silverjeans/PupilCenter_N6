/**
 * @file    pupil_detect.c
 * @brief   Fast G-channel pupil detector. No full-frame grayscale
 *          conversion; no malloc. All working memory is static.
 *
 * Speed notes applied here:
 *   - ROI is walked exactly once for thresholding; the inner loop reads
 *     the RGB565 halfword and extracts G with a single shift and mask.
 *   - Flood-fill uses an explicit 4-connectivity stack (int16 index pairs
 *     re-encoded as uint32) so there is no recursion, no push per
 *     neighbour that is already zero.
 *   - The mask buffer is mutated during flood fill (visited pixel set to 0)
 *     so no separate visited bitmap is required.
 *   - Centroid uses integer division once per blob, not per pixel.
 *   - Candidate scoring uses Chebyshev distance (no sqrt, no hypot).
 *   - All loops avoid multiply inside by keeping a running row pointer.
 */
#include "pupil_detect.h"

#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Static working memory.                                             */
/*                                                                    */
/* MAX_ROI = 256 x 256 -> mask = 64 KB, stack = 256 KB worst case.    */
/* The flood-fill stack is sized for the pathological "the entire ROI */
/* is one blob" case; 2 bytes per x, 2 bytes per y -> 4 B per entry.  */
/* ------------------------------------------------------------------ */
static uint8_t  s_mask[PUPIL_DETECT_MAX_PIXELS];
static uint32_t s_stack[PUPIL_DETECT_MAX_PIXELS];
static pupil_cfg_t s_cfg;

/* Pack (x,y) into a uint32 stack entry and unpack. */
static inline uint32_t pack_xy(uint16_t x, uint16_t y)
{
    return ((uint32_t)x) | (((uint32_t)y) << 16);
}
static inline void unpack_xy(uint32_t v, uint16_t* x, uint16_t* y)
{
    *x = (uint16_t)(v & 0xFFFFu);
    *y = (uint16_t)(v >> 16);
}

/* ------------------------------------------------------------------ */
/* Init                                                               */
/* ------------------------------------------------------------------ */
int pupil_detect_init(const pupil_cfg_t* cfg)
{
    if (!cfg) return -1;
    s_cfg = *cfg;
    if (s_cfg.max_blobs == 0 || s_cfg.max_blobs > CFG_DETECT_MAX_BLOBS) {
        s_cfg.max_blobs = CFG_DETECT_MAX_BLOBS;
    }
    return 0;
}

/* ================================================================== */
/* 1. Threshold on G, reading directly from the RGB frame.            */
/* ================================================================== */

/* RGB565 little-endian: byte0 = RRRRRGGG, byte1 = GGGBBBBB.
 * G occupies bits 10..5 of the 16-bit pixel (6 bits, 0..63).
 * We left-shift by 2 to map it onto 0..252 so the caller's threshold
 * stays on a familiar 0..255 scale. */
static inline uint8_t g8_from_rgb565(const uint8_t* p)
{
    uint16_t px = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return (uint8_t)(((px >> 5) & 0x3Fu) << 2);
}

/* RGB888 stored as R,G,B bytes. */
static inline uint8_t g8_from_rgb888(const uint8_t* p)
{
    return p[1];
}

int threshold_dark_region_g(const frame_t* frame,
                            const roi_t*   roi,
                            uint8_t        thr,
                            uint8_t*       dst_mask)
{
    if (!frame || !roi || !dst_mask) return -1;

    const uint16_t w = roi->w;
    const uint16_t h = roi->h;
    const uint16_t stride = frame->stride;
    const uint8_t* base = frame->data + (uint32_t)roi->y * stride;

    if (frame->fmt == PIX_FMT_RGB565) {
        const uint32_t xoff = (uint32_t)roi->x * 2u;   /* 2 bytes / pixel */
        for (uint16_t y = 0; y < h; ++y) {
            const uint8_t* src = base + xoff;
            uint8_t*       dst = dst_mask + (uint32_t)y * w;

            /* Unrolled by 4. Compilers often fail to unroll across the
             * RGB565->G extraction, so do it by hand. */
            uint16_t x = 0;
            for (; x + 4 <= w; x += 4) {
                uint8_t g0 = g8_from_rgb565(src + 0);
                uint8_t g1 = g8_from_rgb565(src + 2);
                uint8_t g2 = g8_from_rgb565(src + 4);
                uint8_t g3 = g8_from_rgb565(src + 6);
                dst[0] = (g0 <= thr);
                dst[1] = (g1 <= thr);
                dst[2] = (g2 <= thr);
                dst[3] = (g3 <= thr);
                src += 8;
                dst += 4;
            }
            for (; x < w; ++x) {
                uint8_t g = g8_from_rgb565(src);
                *dst++ = (g <= thr);
                src += 2;
            }
            base += stride;
        }
        return 0;
    }

    if (frame->fmt == PIX_FMT_RGB888) {
        const uint32_t xoff = (uint32_t)roi->x * 3u;   /* 3 bytes / pixel */
        for (uint16_t y = 0; y < h; ++y) {
            const uint8_t* src = base + xoff;
            uint8_t*       dst = dst_mask + (uint32_t)y * w;
            for (uint16_t x = 0; x < w; ++x) {
                *dst++ = (g8_from_rgb888(src) <= thr);
                src += 3;
            }
            base += stride;
        }
        return 0;
    }

    if (frame->fmt == PIX_FMT_GRAY8) {
        const uint8_t* s0 = base + roi->x;
        for (uint16_t y = 0; y < h; ++y) {
            const uint8_t* src = s0 + (uint32_t)y * stride;
            uint8_t*       dst = dst_mask + (uint32_t)y * w;
            for (uint16_t x = 0; x < w; ++x) dst[x] = (src[x] <= thr);
        }
        return 0;
    }

    return -2;
}

/* ================================================================== */
/* 2. Flood-fill connected components                                 */
/* ================================================================== */

static void flood_fill_one(uint8_t* mask, uint16_t w, uint16_t h,
                           uint16_t sx, uint16_t sy,
                           blob_stat_t* out)
{
    out->area   = 0;
    out->sum_x  = 0;
    out->sum_y  = 0;
    out->bbox_x0 = sx; out->bbox_y0 = sy;
    out->bbox_x1 = sx; out->bbox_y1 = sy;

    uint32_t* sp = s_stack;
    *sp++ = pack_xy(sx, sy);
    mask[(uint32_t)sy * w + sx] = 0;   /* mark visited */

    while (sp != s_stack) {
        uint16_t x, y;
        unpack_xy(*--sp, &x, &y);

        out->area  += 1u;
        out->sum_x += x;
        out->sum_y += y;
        if (x < out->bbox_x0) out->bbox_x0 = x;
        if (x > out->bbox_x1) out->bbox_x1 = x;
        if (y < out->bbox_y0) out->bbox_y0 = y;
        if (y > out->bbox_y1) out->bbox_y1 = y;

        /* 4-connectivity neighbours. Cheaper than 8-conn and just as
         * good for compact pupil blobs. */
        /* right */
        if (x + 1u < w && mask[(uint32_t)y * w + (x + 1u)]) {
            mask[(uint32_t)y * w + (x + 1u)] = 0;
            *sp++ = pack_xy(x + 1u, y);
        }
        /* left */
        if (x > 0 && mask[(uint32_t)y * w + (x - 1u)]) {
            mask[(uint32_t)y * w + (x - 1u)] = 0;
            *sp++ = pack_xy(x - 1u, y);
        }
        /* down */
        if (y + 1u < h && mask[(uint32_t)(y + 1u) * w + x]) {
            mask[(uint32_t)(y + 1u) * w + x] = 0;
            *sp++ = pack_xy(x, y + 1u);
        }
        /* up */
        if (y > 0 && mask[(uint32_t)(y - 1u) * w + x]) {
            mask[(uint32_t)(y - 1u) * w + x] = 0;
            *sp++ = pack_xy(x, y - 1u);
        }
    }
}

int find_connected_components_floodfill(uint8_t* mask,
                                        uint16_t w, uint16_t h,
                                        blob_stat_t* stats,
                                        uint16_t max_blobs,
                                        uint16_t* n_blobs)
{
    if (!mask || !stats || !n_blobs) return -1;

    uint16_t found = 0;
    const uint32_t n = (uint32_t)w * (uint32_t)h;

    /* Linear scan. The first time we meet a set pixel, we flood-fill from
     * there. Flood fill clears the blob so subsequent scan cells of that
     * blob will be zero and skipped. */
    for (uint32_t i = 0; i < n; ++i) {
        if (!mask[i]) continue;
        if (found >= max_blobs) {
            /* Drain remaining blobs by clearing to avoid spending cycles
             * on them next time. Cheap early-exit. */
            *n_blobs = found;
            return -1;
        }
        uint16_t sx = (uint16_t)(i % w);
        uint16_t sy = (uint16_t)(i / w);
        flood_fill_one(mask, w, h, sx, sy, &stats[found]);
        ++found;
    }

    *n_blobs = found;
    return 0;
}

/* ================================================================== */
/* 3. Candidate selection                                             */
/* ================================================================== */

/* Smooth area score: 255 when area == (min+max)/2, tapering to 0 at the
 * gates. Keeps arithmetic in fixed point, no sqrt, no div-by-zero. */
static uint8_t area_score(uint32_t area, uint32_t lo, uint32_t hi)
{
    if (area < lo || area > hi) return 0;
    uint32_t mid  = (lo + hi) / 2u;
    uint32_t dist = (area > mid) ? (area - mid) : (mid - area);
    uint32_t span = (hi - lo) / 2u;
    if (span == 0) return 255;
    if (dist >= span) return 0;
    return (uint8_t)(255u - (dist * 255u / span));
}

/* Chebyshev distance from blob centroid to ROI center, 0..255 where 255
 * means "right at the center". */
static uint8_t center_score(const blob_stat_t* b,
                            uint16_t roi_w, uint16_t roi_h)
{
    if (b->area == 0) return 0;
    uint32_t cx = b->sum_x / b->area;
    uint32_t cy = b->sum_y / b->area;
    uint32_t ox = (uint32_t)roi_w / 2u;
    uint32_t oy = (uint32_t)roi_h / 2u;
    uint32_t dx = (cx > ox) ? (cx - ox) : (ox - cx);
    uint32_t dy = (cy > oy) ? (cy - oy) : (oy - cy);
    uint32_t dmax = (dx > dy) ? dx : dy;
    uint32_t max_span = (ox > oy) ? ox : oy;
    if (max_span == 0) return 128;
    if (dmax >= max_span) return 0;
    return (uint8_t)(255u - (dmax * 255u / max_span));
}

int select_best_pupil_candidate(const blob_stat_t* stats,
                                uint16_t n_blobs,
                                uint16_t roi_w,
                                uint16_t roi_h,
                                const pupil_cfg_t* cfg,
                                uint16_t* best_idx,
                                uint8_t*  best_score)
{
    if (best_idx)   *best_idx   = 0;
    if (best_score) *best_score = 0;
    if (!stats || !cfg || n_blobs == 0) return 0;

    uint16_t winner   = 0xFFFF;
    uint32_t winner_s = 0;

    /* bias in [0..100]. 0 = area only, 100 = heavy center pull. */
    uint32_t cbias = cfg->center_bias_x100;
    if (cbias > 100u) cbias = 100u;
    uint32_t abias = 100u - cbias;

    for (uint16_t i = 0; i < n_blobs; ++i) {
        uint8_t  a = area_score(stats[i].area, cfg->min_area_px, cfg->max_area_px);
        if (a == 0) continue;
        uint8_t  c = center_score(&stats[i], roi_w, roi_h);

        /* weighted integer score, normalised to 0..255 */
        uint32_t s = (abias * a + cbias * c) / 100u;
        if (s > winner_s) { winner_s = s; winner = i; }
    }

    if (winner == 0xFFFF) return 0;
    if (best_idx)   *best_idx   = winner;
    if (best_score) *best_score = (uint8_t)winner_s;
    return 1;
}

/* ================================================================== */
/* 4. Centroid / result assembly - only place that adds the ROI origin */
/* ================================================================== */

void compute_centroid(const blob_stat_t* stat,
                      const roi_t*       roi,
                      uint8_t            score,
                      pupil_result_t*    out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (roi) out->roi_used = *roi;
    if (!stat || stat->area == 0 || !roi) {
        out->valid = 0;
        return;
    }

    /* One integer division for x, one for y. */
    uint32_t cx_local = stat->sum_x / stat->area;
    uint32_t cy_local = stat->sum_y / stat->area;
    uint32_t bw = (uint32_t)(stat->bbox_x1 - stat->bbox_x0 + 1u);
    uint32_t bh = (uint32_t)(stat->bbox_y1 - stat->bbox_y0 + 1u);
    uint32_t d  = (bw < bh) ? bw : bh;     /* short axis -> diameter estimate */

    out->cx         = (int16_t)((int32_t)cx_local + roi->x);
    out->cy         = (int16_t)((int32_t)cy_local + roi->y);
    out->radius_px  = (uint16_t)(d >> 1);
    out->area_px    = stat->area;
    out->confidence = score;
    out->valid      = 1;
}

/* ================================================================== */
/* 5. Top-level - wire the stages                                     */
/* ================================================================== */

int pupil_detect_run(const frame_t* frame,
                     const roi_t*   roi,
                     pupil_result_t* out)
{
    if (!frame || !roi || !out) {
        if (out) { memset(out, 0, sizeof *out); }
        return 0;
    }

    memset(out, 0, sizeof *out);
    out->roi_used = *roi;

    if (roi->w == 0 || roi->h == 0 ||
        roi->w > PUPIL_DETECT_MAX_ROI_W ||
        roi->h > PUPIL_DETECT_MAX_ROI_H) {
        return 0;
    }

    /* Stage 1: threshold -> mask (ROI-local, no full-frame buffer). */
    if (threshold_dark_region_g(frame, roi, s_cfg.dark_threshold, s_mask) != 0) {
        return 0;
    }

    /* Stage 2: flood-fill up to N blobs. */
    blob_stat_t blobs[CFG_DETECT_MAX_BLOBS];
    uint16_t    n_blobs = 0;
    (void)find_connected_components_floodfill(s_mask, roi->w, roi->h,
                                              blobs,
                                              s_cfg.max_blobs,
                                              &n_blobs);
    if (n_blobs == 0) return 0;

    /* Stage 3: pick the best candidate. */
    uint16_t idx;
    uint8_t  score;
    if (!select_best_pupil_candidate(blobs, n_blobs, roi->w, roi->h,
                                     &s_cfg, &idx, &score)) {
        return 0;
    }

    /* Stage 4: full-frame centroid + fill result. */
    compute_centroid(&blobs[idx], roi, score, out);
    return out->valid ? 1 : 0;
}
