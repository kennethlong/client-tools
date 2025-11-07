#pragma once

// --- Core foundation types ---
#include "sharedFoundation/NetworkIdArchive.h"

// --- Math types ---
#include "sharedMathArchive/QuaternionArchive.h"
#include "sharedMathArchive/SphereArchive.h"
#include "sharedMathArchive/TransformArchive.h"
#include "sharedMathArchive/VectorArchive.h"

// --- Unicode / Localization ---
#include "unicodeArchive/UnicodeArchive.h"
#include "localizationArchive/StringIdArchive.h"

// --- Shared Game types ---
#include "sharedGame/AttribModArchive.h"
#include "sharedGame/AuctionTokenArchive.h"
#include "sharedGame/CraftingDataArchive.h"
#include "sharedGame/ProsePackageArchive.h"
#include "sharedGame/ProsePackageParticipantArchive.h"
#include "sharedGame/StartingLocationDataArchive.h"
#include "sharedGame/SuiCommandArchive.h"
#include "sharedGame/SuiPageDataArchive.h"

// --- Shared Object ---
#include "sharedObject/CachedNetworkIdArchive.h"
#include "sharedObject/SlotIdArchive.h"

// --- Shared Skill System ---
#include "sharedSkillSystem/SkillObjectArchive.h"

// --- Shared Utility ---
#include "sharedUtility/LocationArchive.h"
#include "sharedUtility/ValueDictionaryArchive.h"
#include "sharedUtility/StartingLocationDataArchive.h"
#include "sharedUtility/InstallationResourceDataArchive.h"

// --- NetworkMessages Archives you actually need ---
#include "sharedNetworkMessages/chat/ChatAvatarIdArchive.h"
#include "sharedNetworkMessages/chat/ChatRoomDataArchive.h"
#include "sharedNetworkMessages/clientGameServer/MapLocationArchive.h"
#include "sharedNetworkMessages/planetWatch/ServerInfoArchive.h"

// --- Your custom message archives ---
#include "swgSharedNetworkMessages/survey/SurveyMessageArchive.h"
#include "swgSharedNetworkMessages/client/ClientCentralMessagesArchive.h"
