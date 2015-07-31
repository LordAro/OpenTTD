/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file extended_ver_sl.cpp Functions related to handling save/load extended version info. */

#include "../stdafx.h"
#include "../debug.h"
#include "extended_ver_sl.h"

#include <vector>

#include "../safeguards.h"

uint16 _sl_xv_feature_versions[XSLFI_SIZE];                 ///< array of all known feature types and their current versions
std::vector<uint32> _sl_xv_discardable_chunk_ids;           ///< list of chunks IDs which we can discard if no chunk loader exists

/**
 * Extended save/load feature test
 *
 * First performs a tradional check on the provided @p savegame_version against @p savegame_version_from and @p savegame_version_to.
 * Then, if the feature set in the constructor is not XSLFI_NULL, also check than the feature version is inclusively bounded by @p min_version and @p max_version,
 * and return the combination of the two tests using the operator defined in the constructor.
 * Otherwise just returns the result of the savegame version test
 */
bool SlXvFeatureTest::IsFeaturePresent(SaveLoadVersion savegame_version, SaveLoadVersion savegame_version_from, SaveLoadVersion savegame_version_to) const
{
	bool savegame_version_ok = savegame_version >= savegame_version_from && savegame_version <= savegame_version_to;

	if (this->feature == XSLFI_NULL) return savegame_version_ok;

	bool feature_ok = SlXvIsFeaturePresent(this->feature, this->min_version, this->max_version);

	switch (op) {
		case XSLFTO_OR:
			return savegame_version_ok || feature_ok;

		case XSLFTO_AND:
			return savegame_version_ok && feature_ok;

		default:
			NOT_REACHED();
			return false;
	}
}

/**
 * Returns true if @p feature is present and has a version inclusively bounded by @p min_version and @p max_version
 */
bool SlXvIsFeaturePresent(SlXvFeatureIndex feature, uint16 min_version, uint16 max_version)
{
	assert(feature < XSLFI_SIZE);
	return _sl_xv_feature_versions[feature] >= min_version && _sl_xv_feature_versions[feature] <= max_version;
}

/**
 * Resets all extended feature versions to 0
 */
void SlXvResetState()
{
	memset(_sl_xv_feature_versions, 0, sizeof(_sl_xv_feature_versions));
}

/**
 * Resets all extended feature versions to their currently enabled versions, i.e. versions suitable for saving
 */
void SlXvSetCurrentState()
{
	extern bool _sl_is_ext_version;

	SlXvResetState();
	_sl_is_ext_version = true;

	// TODO: set versions for currently enabled features here
}

/**
 * Check for "special" savegame versions (i.e. known patchpacks) and set correct savegame version, settings, etc.
 */
void SlXvCheckSpecialSavegameVersions()
{
	// Checks for special savegame versions go here
}

/**
 * Return true if this chunk has been marked as discardable
 */
bool SlXvIsChunkDiscardable(uint32 id)
{
	for (size_t i = 0; i < _sl_xv_discardable_chunk_ids.size(); i++) {
		if (_sl_xv_discardable_chunk_ids[i] == id) {
			return true;
		}
	}
	return false;
}
