// SPDX-License-Identifier: AGPL-3.0-or-later
//
// WebGL context memoization for the emscripten build (--pre-js, runs on the
// page AND on the pthread worker where the game canvas lives).
//
// Firefox 159 / ESR 153.6 (Bug 2079676) throws InvalidStateError
// "Cannot update created context to different thread" on ANY getContext call
// after the first one on an OffscreenCanvas owned by a worker - even on the
// same thread that created the context (relaxed in Firefox 160, Bug 2082349,
// not uplifted to 159/ESR 153). The client legitimately asks for the canvas
// context more than once during startup (SDL_GL_CreateContext adopting the
// bootstrap context, then the renderer's fallback emscripten_webgl_create_
// context), and each of those becomes a getContext call on the transferred
// OffscreenCanvas - which kills the worker on Firefox 159.
//
// A canvas can only ever have ONE WebGL2 context (per the WebGL spec the
// browser returns that same object for every subsequent getContext of the
// same type, attributes are ignored), so handing the first context back
// unchanged is exactly what the browser itself does on every other engine.
'use strict';
(function () {
  if (typeof OffscreenCanvas === 'undefined') return;
  var proto = OffscreenCanvas.prototype;
  if (proto.__xuoContextCached) return;
  var orig = proto.getContext;
  if (!orig) return;
  proto.getContext = function (type, attrs) {
    // per-type memo: webgl/webgl2 are the ones created more than once here;
    // 2d/bitmaps stay untouched (different context kinds never coexist with
    // a webgl context per spec, but keep the cache type-scoped anyway)
    var key = 'webgl2' === type ? 'webgl2' : 'webgl' === type ? 'webgl' : null;
    if (key && this['__xuoCtx_' + key]) return this['__xuoCtx_' + key];
    var ctx = orig.call(this, type, attrs);
    if (key && ctx) this['__xuoCtx_' + key] = ctx;
    return ctx;
  };
  proto.__xuoContextCached = true;
})();
