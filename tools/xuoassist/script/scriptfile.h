// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script file engine (was script/fileengine.*, CScriptFileObject over
// QFile/QTextStream): stdio-based, script-thread owned.
//
// SANDBOX (the plan's security rule; the Qt engine accepted any path):
// every path a script passes is normalized and resolved under the sandbox
// root - the client directory (the config root, CAssistantConfig::GetRootPath:
// the client cwd). Relative paths join the root; absolute paths are accepted
// only inside it; any ".." component that would climb out of the root (and
// any path that normalizes outside it) is rejected, the attempt is logged,
// and the call fails (Open/Append return false, the text ops do nothing,
// Remove keeps the file). There is deliberately no symlink resolution -
// lexical normalization only.
//
// Threading: file objects live entirely on the calling script worker thread
// (Qt parity: CScriptFileObject used QFile directly, no command round trip).
// The sandbox root is a process constant set once on the main thread at UI
// install; the harness sets its own.

#include "../core/global.h"

namespace xuoassist {

struct XA_SCRIPT_FILE
{
    void *Handle{ nullptr }; // FILE*
    string Path;             // sanitized path of the current handle
};

// Sets the sandbox root (main thread, once at UI install). Empty resets to
// the process cwd (the client's binary dir, which IS the config root).
void ScriptFile_SetSandboxRoot(const string &root);

// Normalizes `path` into the sandbox. Returns "" when the path escapes the
// sandbox (logged with the `api` origin).
string ScriptFile_Sanitize(const string &path, const char *api);

// CScriptFileObject surface. `checkExists` fails the call when the file is
// missing (the Qt overload flag). ReadLine returns "" at end of file; Read
// returns the next whitespace-delimited token (QTextStream operator>>).
bool ScriptFile_Open(XA_SCRIPT_FILE *file, const string &path, bool checkExists);
bool ScriptFile_Append(XA_SCRIPT_FILE *file, const string &path, bool checkExists);
bool ScriptFile_Opened(const XA_SCRIPT_FILE *file);
void ScriptFile_Close(XA_SCRIPT_FILE *file);
string ScriptFile_ReadLine(XA_SCRIPT_FILE *file);
string ScriptFile_Read(XA_SCRIPT_FILE *file);
void ScriptFile_WriteLine(XA_SCRIPT_FILE *file, const string &text);
void ScriptFile_Write(XA_SCRIPT_FILE *file, const string &text);
void ScriptFile_Remove(XA_SCRIPT_FILE *file);
bool ScriptFile_RemovePath(const string &path);

} // namespace xuoassist

