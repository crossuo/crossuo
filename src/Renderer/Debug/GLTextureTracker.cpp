// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#include "GLTextureTracker.h"

#if defined(USE_GL3) || defined(USE_GLES) || defined(USE_GL1)

#include <external/gfx/gfx.h>
#include <common/logging/logging.h>
#include <xuocore/common.h> // LOG_INFO / LOG_ERROR (LOGGER_MODULE scoped)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define LOGGER_MODULE Renderer

// ---------------------------------------------------------------------------
// name table + history
// ---------------------------------------------------------------------------


struct NameRecord
{
    unsigned int name = 0;
    bool live = false;
    unsigned int gen_frame = 0;
    unsigned int del_frame = 0;
    unsigned long del_count = 0; // times deleted since the last gen
    char gen_stack[1536] = { 0 };
    char del_stack[1536] = { 0 };
};

// one event in the creation/deletion history: this is what makes the report
// tell the story leading up to the failing bind (names are recycled by the
// driver, so the current live/dead bit alone is not enough)
enum class EventKind : uint8_t
{
    Generated,
    Deleted,
};

struct Event
{
    EventKind kind = EventKind::Generated;
    unsigned int name = 0;
    unsigned int frame = 0;
};

static bool s_enabled = false;
static bool s_installed = false;
static bool s_hooked = false;
static bool s_aborted = false;
static unsigned int s_frame = 0;
static unsigned long long s_binds = 0;
static unsigned long long s_bad_binds = 0;

static constexpr size_t HISTORY_MAX = 4096;

// Deliberately LEAKED containers: the game's global/static textures tear down
// through __cxa_finalize in unspecified cross-TU order, and delete/bind events
// still arrive while (or after) this translation unit's statics are destroyed.
// A file-scope std::vector would be dead memory by then (SIGSEGV in the
// wrappers at exit); heap-allocating and never freeing keeps them valid for
// the whole process lifetime, which is exactly the lifetime being tracked.
static std::vector<NameRecord> *s_records = nullptr;
static std::vector<Event> *s_history = nullptr;

static std::vector<NameRecord> &records()
{
    static std::vector<NameRecord> *leaked = new std::vector<NameRecord>();
    if (s_records == nullptr)
    {
        s_records = leaked;
        s_records->reserve(8192);
    }
    return *s_records;
}

static std::vector<Event> &history()
{
    static std::vector<Event> *leaked = new std::vector<Event>();
    if (s_history == nullptr)
    {
        s_history = leaked;
        s_history->reserve(HISTORY_MAX);
    }
    return *s_history;
}

static bool logging_enabled()
{
    static const bool env = getenv("XUO_TRACK_TEXTURES") != nullptr;
    return env;
}

// forward decls (both are defined further down; they must see NameRecord, so
// they are declared after its definition)
static void dump_provenance_and_history(const NameRecord *rec);
static void report_failure(const char *statement, unsigned int error);
static void report_bad_bind(const char *what, unsigned int name);

static void capture_stack(char *out, size_t size)
{
    if (size == 0)
    {
        return;
    }
    out[0] = '\0';
    const loguru::Text st = loguru::stacktrace(3);
    if (st.empty())
    {
        snprintf(out, size, "(no stacktrace: LOGURU_STACKTRACES off)");
        return;
    }
    snprintf(out, size, "%s", st.c_str());
}

static NameRecord *record_for(unsigned int name)
{
    for (auto &r : records())
    {
        if (r.name == name)
        {
            return &r;
        }
    }
    return nullptr;
}

#if defined(USE_GL3W) // only the gl3w hooks below use these
static NameRecord &record_slot(unsigned int name)
{
    if (auto *found = record_for(name))
    {
        return *found;
    }
    NameRecord rec;
    rec.name = name;
    records().push_back(rec);
    return records().back();
}

static void push_event(EventKind kind, unsigned int name)
{
    auto &hist = history();
    if (hist.size() >= HISTORY_MAX)
    {
        hist.erase(hist.begin(), hist.begin() + HISTORY_MAX / 2);
    }
    Event ev;
    ev.kind = kind;
    ev.name = name;
    ev.frame = s_frame;
    hist.push_back(ev);
}
#endif // USE_GL3W

// ---------------------------------------------------------------------------
// entry point wrappers (gl3w only: GLEW's are real symbols, not pointers)
// ---------------------------------------------------------------------------

#if defined(USE_GL3W)
typedef void (*PFN_TRACKED_GEN)(GLsizei, GLuint *);
typedef void (*PFN_TRACKED_DELETE)(GLsizei, const GLuint *);

static PFN_TRACKED_GEN s_real_gen = nullptr;
static PFN_TRACKED_DELETE s_real_delete = nullptr;
#endif // USE_GL3W

#ifndef APIENTRY
#define APIENTRY
#endif

#if defined(USE_GL3W)
#define GLTEXTURE_TRACKER_HAS_HOOKS 1

void APIENTRY tracked_glGenTextures(GLsizei n, GLuint *textures)
{
    if (s_real_gen)
    {
        s_real_gen(n, textures);
    }
    if (!s_enabled || textures == nullptr)
    {
        return;
    }
    for (GLsizei i = 0; i < n; ++i)
    {
        auto &rec = record_slot(textures[i]);
        // a recycled name must forget its previous life, or the delete
        // provenance reported for a later failure would be stale
        rec.live = true;
        rec.gen_frame = s_frame;
        rec.del_count = 0;
        rec.del_frame = 0;
        rec.del_stack[0] = '\0';
        capture_stack(rec.gen_stack, sizeof(rec.gen_stack));
        push_event(EventKind::Generated, textures[i]);
    }
}

void APIENTRY tracked_glDeleteTextures(GLsizei n, const GLuint *textures)
{
    if (s_enabled && textures != nullptr)
    {
        for (GLsizei i = 0; i < n; ++i)
        {
            auto *rec = record_for(textures[i]);
            if (rec != nullptr && rec->live)
            {
                rec->live = false;
                rec->del_frame = s_frame;
                rec->del_count++;
                capture_stack(rec->del_stack, sizeof(rec->del_stack));
                push_event(EventKind::Deleted, textures[i]);
            }
        }
    }
    if (s_real_delete)
    {
        s_real_delete(n, textures);
    }
}
#endif // USE_GL3W

static const char *event_kind_as_string(EventKind kind)
{
    return kind == EventKind::Generated ? "GEN" : "DEL";
}

// short, allocation-free description of a name's tracked state (used inside
// log formatting)
static const char *describe_name(unsigned int name)
{
    const NameRecord *rec = record_for(name);
    if (rec == nullptr)
    {
        return "UNTRACKED";
    }
    return rec->live ? "LIVE" : "DELETED";
}

// the shared tail of every failure report: who generated/deleted the name and
// the chronological history that led here
static void dump_provenance_and_history(const NameRecord *rec)
{
    if (rec != nullptr)
    {
        LOG_ERROR(
            "----------------------------------------\n"
            "NAME %u LAST GENERATION (frame %u)\n%s\n"
            "----------------------------------------\n"
            "NAME %u LAST DELETION (frame %u, deleted %lu times since that gen)\n%s",
            rec->name,
            rec->gen_frame,
            rec->gen_stack,
            rec->name,
            rec->del_frame,
            rec->del_count,
            rec->del_stack);
    }
    else
    {
        LOG_ERROR("----------------------------------------\n(no usable name record)");
    }

    // names are recycled by the driver, so only the sequence shows how the
    // dead/stale name the failure used came to be
    const size_t shown = history().size() < 1200 ? history().size() : (size_t)1200;
    LOG_ERROR(
        "----------------------------------------\nHISTORY (%zu events, showing last %zu)",
        history().size(),
        shown);
    const size_t first = history().size() - shown;
    for (size_t i = first; i < history().size(); ++i)
    {
        const auto &ev = history()[i];
        LOG_ERROR("  [f%u] %s %u", ev.frame, event_kind_as_string(ev.kind), ev.name);
    }
    LOG_ERROR("================ END TEXTURE NAME TRACKER ================");
}

// called from GL_CHECK (external/gfx/gfx.h) right before the abort: prints the
// full story for whatever the failing statement was doing, then lets the caller
// abort so only the FIRST failure is ever seen
extern "C" void GLTextureTracker_OnGLError(const char *statement, unsigned int error)
{
    report_failure(statement, error);
}

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------
void GLTextureTracker_Install()
{
    if (s_installed)
    {
        return;
    }
    s_installed = true;
    s_enabled = logging_enabled();
    records();
    history();

#if defined(USE_GL3W)
    s_real_gen = gl3wGenTextures;
    s_real_delete = gl3wDeleteTextures;
    gl3wGenTextures = tracked_glGenTextures;
    gl3wDeleteTextures = tracked_glDeleteTextures;
    s_hooked = true;
#else
    s_hooked = false;
#endif

    LOG_INFO(
        "textrack: %s (gen/delete hooks: %s) - the full story is printed only on"
        " the first failure, then the process aborts",
        s_enabled ? "enabled" : "disabled (XUO_TRACK_TEXTURES)",
        s_hooked ? "on" : "off");
}

void GLTextureTracker_OnBind(const char *what, unsigned int name)
{
    if (!s_enabled)
    {
        return;
    }
    s_binds++;
    auto *rec = record_for(name);
    if (rec != nullptr && rec->live)
    {
        return; // healthy bind: nothing to say, keep the log quiet
    }
    s_bad_binds++;
    report_bad_bind(what, name);
}

static void report_bad_bind(const char *what, unsigned int name)
{
    if (!s_enabled || s_aborted)
    {
        return;
    }
    s_aborted = true; // one story only: the first failure is the one to read

    const NameRecord *rec = record_for(name);
    char bind_stack[1536];
    capture_stack(bind_stack, sizeof(bind_stack));

    LOG_ERROR(
        "================ TEXTURE NAME FAILURE ================\n"
        "bind of %s name %u from %s\n"
        "bind #%llu (bad #%llu) at frame %u\n"
        "name state: %s\n"
        "----------------------------------------\n"
        "BIND STACK\n%s",
        rec == nullptr ? "NEVER-GENERATED" : "DELETED",
        name,
        what ? what : "?",
        s_binds,
        s_bad_binds,
        s_frame,
        rec == nullptr ? "no record (never passed through glGenTextures)"
                       : (rec->live ? "record says LIVE (name may be bound to the wrong target)"
                                    : "record says DELETED"),
        bind_stack);
    dump_provenance_and_history(rec);

    // a bad bind is fatal: stop at the first one so the log holds a single,
    // readable story (XUO_GL_NO_ABORT=1 to keep running instead)
    if (getenv("XUO_GL_NO_ABORT") == nullptr)
    {
        abort();
    }
}

// the GL_CHECK failure path: the driver already told us which statement failed,
// so pull the name it was operating on straight from the live GL state (the
// name the statement tried to bind) plus everything about the unit it targets
static void report_failure(const char *statement, unsigned int error)
{
    if (!s_enabled || s_aborted)
    {
        return;
    }
    s_aborted = true;

    GLint activeUnit = -1;
    GLint bound2D = -1;
    GLint boundArray = -1;
    GLint boundCubemap = -1;
    GLint bound3D = -1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeUnit);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound2D);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &boundArray);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &boundCubemap);
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound3D);

    char where[1536];
    capture_stack(where, sizeof(where));

    LOG_ERROR(
        "================ GL ERROR, TEXTURE STATE ================\n"
        "failing statement: %s\n"
        "glGetError: 0x%04x%s\n"
        "frame %u, binds so far %llu (bad %llu)\n"
        "active unit: %d\n"
        "  bound GL_TEXTURE_2D       = %d  (isTexture: %d, tracked: %s)\n"
        "  bound GL_TEXTURE_2D_ARRAY = %d  (isTexture: %d, tracked: %s)\n"
        "  bound GL_TEXTURE_CUBE_MAP = %d  (isTexture: %d)\n"
        "  bound GL_TEXTURE_3D       = %d  (isTexture: %d)\n"
        "----------------------------------------\n"
        "STACK\n%s",
        statement ? statement : "?",
        error,
        error == 0x0502u ? " (GL_INVALID_OPERATION)" : "",
        s_frame,
        s_binds,
        s_bad_binds,
        activeUnit,
        bound2D,
        glIsTexture(GLuint(bound2D)),
        describe_name(GLuint(bound2D)),
        boundArray,
        glIsTexture(GLuint(boundArray)),
        describe_name(GLuint(boundArray)),
        boundCubemap,
        glIsTexture(GLuint(boundCubemap)),
        bound3D,
        glIsTexture(GLuint(bound3D)),
        where);

    // report both targets: the 2D one is what a "non-gen name" 2D bind trips,
    // the array is the streaming/art-array suspect
    for (unsigned int candidate : { GLuint(bound2D), GLuint(boundArray) })
    {
        if (candidate != 0)
        {
            dump_provenance_and_history(record_for(candidate));
        }
    }
}

void GLTextureTracker_OnFrame()
{
    if (!s_enabled)
    {
        return;
    }
    s_frame++;
}

void GLTextureTracker_Dump()
{
    if (!s_enabled)
    {
        return;
    }
    size_t live = 0;
    for (const auto &r : records())
    {
        if (r.live)
        {
            live++;
        }
    }
    LOG_INFO(
        "textrack: frame %u: %zu names tracked, %zu live, %llu binds (%llu bad)",
        s_frame,
        records().size(),
        live,
        s_binds,
        s_bad_binds);
}

#endif
