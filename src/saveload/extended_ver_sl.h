/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file extended_ver_sl.h Functions/types related to handling save/load extended version info. */

#ifndef EXTENDED_VER_SL_H
#define EXTENDED_VER_SL_H

#include "../stdafx.h"
#include "../core/bitmath_func.hpp"

// Forward declaration to avoid circular dependency
enum SaveLoadVersion : uint16;

/**
 * List of extended features, each feature has its own (16 bit) version
 */
enum SlXvFeatureIndex {
	XSLFI_NULL                          = 0,      ///< Unused value, to indicate that no extended feature test is in use

	XSLFI_SIZE,                                   ///< Total count of features, including null feature
};

extern uint16 _sl_xv_feature_versions[XSLFI_SIZE];

/**
 * Operator to use when combining traditional savegame number test with an extended feature version test
 */
enum SlXvFeatureTestOperator {
	XSLFTO_OR                           = 0,      ///< Test if traditional savegame version is in bounds OR extended feature is in version bounds
	XSLFTO_AND                                    ///< Test if traditional savegame version is in bounds AND extended feature is in version bounds
};

/**
 * Structure to describe an extended feature version test, and how it combines with a traditional savegame version test
 */
struct SlXvFeatureTest {
	private:
	uint16 min_version;
	uint16 max_version;
	SlXvFeatureIndex feature;
	SlXvFeatureTestOperator op;

	public:
	SlXvFeatureTest()
			: min_version(0), max_version(0), feature(XSLFI_NULL), op(XSLFTO_OR) { }

	SlXvFeatureTest(SlXvFeatureTestOperator op_, SlXvFeatureIndex feature_, uint16 min_version_ = 1, uint16 max_version_ = 0xFFFF)
			: min_version(min_version_), max_version(max_version_), feature(feature_), op(op_) { }

	bool IsFeaturePresent(SaveLoadVersion savegame_version, SaveLoadVersion savegame_version_from, SaveLoadVersion savegame_version_to) const;
};

bool SlXvIsFeaturePresent(SlXvFeatureIndex feature, uint16 min_version = 1, uint16 max_version = 0xFFFF);

/**
 * Returns true if @p feature is missing (i.e. has a version of 0)
 */
inline bool SlXvIsFeatureMissing(SlXvFeatureIndex feature)
{
	return !SlXvIsFeaturePresent(feature);
}

void SlXvResetState();

void SlXvSetCurrentState();

void SlXvCheckSpecialSavegameVersions();

bool SlXvIsChunkDiscardable(uint32 id);

#endif /* EXTENDED_VER_SL_H */
