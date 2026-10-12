// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#pragma once
#include "crg/crg.hpp"
#include "crg/discovery/universal_anchor.hpp"

// State that two plugins define independently and that must resolve to ONE object once both are loaded.
// The host does not define the anchor: it can only be shared between the plugins themselves.
struct SharedState {};
CRG_DECLARE_SHARED_STATE(SharedState)
