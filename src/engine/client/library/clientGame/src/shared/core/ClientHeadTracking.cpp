// ======================================================================
//
// ClientHeadTracking.cpp
// asommers
//
// copyright 2004, sony online entertainment
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ClientHeadTracking.h"

// ======================================================================
//
// NaturalPoint TrackIR support removed in DECRUFT-01 (v2.1 Decruft, Phase 12).
//
// The trackIR/ orphan directory (NPClient.h + the NaturalPoint head-tracking
// SDK) was deleted. This file formerly probed HKCU for the NPClient location,
// LoadLibrary'd NPClient.dll, and polled yaw/pitch from the TrackIR device.
// None of that is supported any more, so the implementation below is reduced
// to no-op stubs. The public interface is retained so its callers
// (CockpitCamera, SetupClientGame, SwgCuiOptControls) keep linking — the
// "checkTrackIr" options checkbox simply reports head tracking as unsupported.
//
// ======================================================================

void ClientHeadTracking::install()
{
}

// ----------------------------------------------------------------------

bool ClientHeadTracking::isSupported()
{
	return false;
}

// ----------------------------------------------------------------------

bool ClientHeadTracking::getEnabled()
{
	return false;
}

// ----------------------------------------------------------------------

void ClientHeadTracking::setEnabled(bool)
{
}

// ----------------------------------------------------------------------

void ClientHeadTracking::getYawAndPitch(float & yaw, float & pitch)
{
	yaw = 0.f;
	pitch = 0.f;
}

// ======================================================================
