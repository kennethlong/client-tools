// ======================================================================
//
// ClientMain.h
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_ClientMain_H
#define INCLUDED_ClientMain_H

// ======================================================================

int ClientMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

// ----------------------------------------------------------------------
// Utinni engine entry-point advertisement (37-01): a distinctly-named,
// external-linkage forwarding shim that calls the file-local crash-fixer
// ClientMainNamespace::installConfigFileOverride(). Named utinni_* so it
// does NOT collide with the unqualified installConfigFileOverride() callers
// inside ClientMain.cpp (which compile under `using namespace
// ClientMainNamespace;`). utinni_advertise.cpp wraps this in the
// config::loadOverrideConfig thunk row (EPA-02 crash-fixer).
// ----------------------------------------------------------------------
void utinni_installConfigFileOverride();

// ======================================================================

#endif
