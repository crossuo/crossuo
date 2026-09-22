# XUO_* environment variable helpers

Development/diagnostic toggles read once at startup. None are required to
play; release behavior is identical with none set. Dumps and logs are
written to the client's current working directory.

Automated benchmark run:

```sh
XUO_AUTO_LOGIN=2,2 XUO_FPS_LOG=1 ./crossuo
```

| Name | When to use | What it does |
|---|---|---|
| `XUO_AUTO_LOGIN=<server>,<character>` | Automated benchmark/validation runs that must reach in-game without input and run uncapped. | Auto-connects with the saved credentials and enters the world on the `<server>`th server, `<character>`th character (1-based positions, skipping empty character slots). Defaults to `2,2` when set but empty or invalid. Also disables frame pacing so an unfocused window does not mask benchmarks. |
| `XUO_FPS_LOG` | Measuring performance. | Prints a 5s summary to stdout: fps, frame avg/min/max ms, and renderer counters (draw calls, vertex uploads, kB uploaded, batch submissions, batched quads). |
| `XUO_DUMP_FRAME` | Capturing a frame's full renderer command stream for offline analysis. | Writes the first rendered frame after startup to `frame<N>.log` (analyze with `tools/renderdump.py`). |
| `XUO_DUMP_FRAME_DELAY=<seconds>` | Capturing an in-game frame in automated runs (startup frames are just login screens). | One-shot dump of the first frame rendered after the given delay; combines with `XUO_AUTO_LOGIN`. |
| `XUO_DEBUG_SCREEN` | Producing a byte-stable golden dump for renderer regression checks. | Replaces the login screen with a deterministic debug screen whose dumps are identical across runs. |
| `XUO_CIRCLE_TRANS` | Exercising the circle-of-transparency path in automated runs. | Forces the circle of transparency on for the session without saving it to the config. |
| `XUO_GUMP_LEGACY` | A/B testing the gump rendering path, or bisecting gump rendering issues. | Disables direct gump drawing (default) and restores the legacy per-gump framebuffer + blit passes. |
| `XUO_BATCH_LOG` | Auditing batching efficiency. | Prints one stderr line per deferred-batch submission: quad count, texture, layer range. |
| `XUO_BATCH_OFF` | Bisecting rendering regressions between the deferred batcher and the draw stream. | Submits every quad/tile immediately instead of accumulating batches (much slower; correctness reference). |
| `XUO_ART_ARRAY_LOG` | Watching the art texture array: placement, layer reuse and long-session growth. | Traces placements (new vs reused layers), layer releases, destroyed sprites, and texture-GC evictions (per list, with age). |
| `XUO_RENDER_MARKERS` | GPU captures / debuggers that benefit from named draw scopes. | Enables `glPushDebugGroup`/`glPopDebugGroup` markers around draw subsystems (~3.3k calls/frame of overhead, off by default). |
| `XUO_SCISSOR_CHECK` | Debugging scissor/clipping issues, especially gump-local scissors. | Validates scissor rects as they are applied and reports violations. |
| `XUO_GL1_TRACE` | Tracing draws through the legacy GL1 renderer. | Prints one warning per textured draw call with texture, position and size. |
| `XUO_GL_VALIDATE=1` | Catching GL errors in a release build (or `=0` to benchmark a debug build). | Overrides the default `glGetError` validation policy (on in debug builds, off in release builds). |
| `XUO_GL_NO_ABORT=1` | Keeping a failing run alive long enough to collect more evidence. | A failed GL validation logs the error (plus the texture tracker's provenance report) instead of aborting the process. |
| `XUO_TRACK_TEXTURES` | Diagnosing invalid/recycled GL texture name usage. | Enables the GL texture name tracker: records create/delete/bind history with stacks; on the first failed bind prints the full story before aborting. |
