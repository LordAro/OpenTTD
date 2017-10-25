/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout.cpp Handling of laying out text. */

#include "stdafx.h"
#include "gfx_layout.h"
#include "string_func.h"
#include "strings_func.h"
#include "debug.h"

#include "table/control_codes.h"

#include "safeguards.h"


/** Cache of ParagraphLayout lines. */
Layouter::LineCache *Layouter::linecache;

/** Cache of Font instances. */
Layouter::FontColourMap Layouter::fonts[FS_END];


/**
 * Construct a new font.
 * @param size   The font size to use for this font.
 * @param colour The colour to draw this font in.
 */
Font::Font(FontSize size, TextColour colour) :
		fc(FontCache::Get(size)), colour(colour)
{
	assert(size < FS_END);
}

Font::~Font()
{
}

/**
 * Helper for getting a ParagraphLayouter of the given type.
 *
 * @note In case no ParagraphLayouter could be constructed, line.layout will be NULL.
 * @param line The cache item to store our layouter in.
 * @param str The string to create a layouter for.
 * @param state The state of the font and color.
 * @tparam T The type of layouter we want.
 */
template <typename T>
static inline void GetLayouter(Layouter::LineCacheItem &line, const char *&str, FontState &state)
{
	if (line.buffer != NULL) free(line.buffer);

	typename T::CharType *buff_begin = MallocT<typename T::CharType>(DRAW_STRING_BUFFER);
	const typename T::CharType *buffer_last = buff_begin + DRAW_STRING_BUFFER;
	typename T::CharType *buff = buff_begin;
	FontMap &fontMapping = line.runs;
	Font *f = Layouter::GetFont(state.fontsize, state.cur_colour);

	line.buffer = buff_begin;

	/*
	 * Go through the whole string while adding Font instances to the font map
	 * whenever the font changes, and convert the wide characters into a format
	 * usable by ParagraphLayout.
	 */
	for (; buff < buffer_last;) {
		WChar c = Utf8Consume(const_cast<const char **>(&str));
		if (c == '\0' || c == '\n') {
			break;
		} else if (c >= SCC_BLUE && c <= SCC_BLACK) {
			state.SetColour((TextColour)(c - SCC_BLUE));
		} else if (c == SCC_PREVIOUS_COLOUR) { // Revert to the previous colour.
			state.SetPreviousColour();
		} else if (c == SCC_TINYFONT) {
			state.SetFontSize(FS_SMALL);
		} else if (c == SCC_BIGFONT) {
			state.SetFontSize(FS_LARGE);
		} else {
			/* Filter out text direction characters that shouldn't be drawn, and
			 * will not be handled in the fallback non ICU case because they are
			 * mostly needed for RTL languages which need more ICU support. */
			if (!T::SUPPORTS_RTL && IsTextDirectionChar(c)) continue;
			buff += AppendToBuffer(buff, buffer_last, c);
			continue;
		}

		if (!fontMapping.Contains(buff - buff_begin)) {
			fontMapping.Insert(buff - buff_begin, f);
		}
		f = Layouter::GetFont(state.fontsize, state.cur_colour);
	}

	/* Better safe than sorry. */
	*buff = '\0';

	if (!fontMapping.Contains(buff - buff_begin)) {
		fontMapping.Insert(buff - buff_begin, f);
	}
	line.layout = GetParagraphLayout(buff_begin, buff, fontMapping);
	line.state_after = state;
}

/**
 * Create a new layouter.
 * @param str      The string to create the layout for.
 * @param maxw     The maximum width.
 * @param colour   The colour of the font.
 * @param fontsize The size of font to use.
 */
Layouter::Layouter(const char *str, int maxw, TextColour colour, FontSize fontsize) : string(str)
{
	FontState state(colour, fontsize);
	WChar c = 0;

	do {
		/* Scan string for end of line */
		const char *lineend = str;
		for (;;) {
			size_t len = Utf8Decode(&c, lineend);
			if (c == '\0' || c == '\n') break;
			lineend += len;
		}

		LineCacheItem& line = GetCachedParagraphLayout(str, lineend - str, state);
		if (line.layout != NULL) {
			/* Line is in cache */
			str = lineend + 1;
			state = line.state_after;
			line.layout->Reflow();
		} else {
			/* Line is new, layout it */
#ifdef WITH_ICU_LAYOUT
			FontState old_state = state;
			const char *old_str = str;

			GetLayouter<ICUParagraphLayout>(line, str, state);
			if (line.layout == NULL) {
				static bool warned = false;
				if (!warned) {
					DEBUG(misc, 0, "ICU layouter bailed on the font. Falling back to the fallback layouter");
					warned = true;
				}

				state = old_state;
				str = old_str;
				GetLayouter<FallbackParagraphLayout>(line, str, state);
			}
#else
			GetLayouter<FallbackParagraphLayout>(line, str, state);
#endif
		}

		/* Copy all lines into a local cache so we can reuse them later on more easily. */
		const ParagraphLayouter::Line *l;
		while ((l = line.layout->NextLine(maxw)) != NULL) {
			*this->Append() = l;
		}

	} while (c != '\0');
}

/**
 * Get the boundaries of this paragraph.
 * @return The boundaries.
 */
Dimension Layouter::GetBounds()
{
	Dimension d = { 0, 0 };
	for (const ParagraphLayouter::Line **l = this->Begin(); l != this->End(); l++) {
		d.width = max<uint>(d.width, (*l)->GetWidth());
		d.height += (*l)->GetLeading();
	}
	return d;
}

/**
 * Get the position of a character in the layout.
 * @param ch Character to get the position of.
 * @return Upper left corner of the character relative to the start of the string.
 * @note Will only work right for single-line strings.
 */
Point Layouter::GetCharPosition(const char *ch) const
{
	/* Find the code point index which corresponds to the char
	 * pointer into our UTF-8 source string. */
	size_t index = 0;
	const char *str = this->string;
	while (str < ch) {
		WChar c;
		size_t len = Utf8Decode(&c, str);
		if (c == '\0' || c == '\n') break;
		str += len;
		index += (*this->Begin())->GetInternalCharLength(c);
	}

	if (str == ch) {
		/* Valid character. */
		const ParagraphLayouter::Line *line = *this->Begin();

		/* Pointer to the end-of-string/line marker? Return total line width. */
		if (*ch == '\0' || *ch == '\n') {
			Point p = { line->GetWidth(), 0 };
			return p;
		}

		/* Scan all runs until we've found our code point index. */
		for (int run_index = 0; run_index < line->CountRuns(); run_index++) {
			const ParagraphLayouter::VisualRun *run = line->GetVisualRun(run_index);

			for (int i = 0; i < run->GetGlyphCount(); i++) {
				/* Matching glyph? Return position. */
				if ((size_t)run->GetGlyphToCharMap()[i] == index) {
					Point p = { (int)run->GetPositions()[i * 2], (int)run->GetPositions()[i * 2 + 1] };
					return p;
				}
			}
		}
	}

	Point p = { 0, 0 };
	return p;
}

/**
 * Get the character that is at a position.
 * @param x Position in the string.
 * @return Pointer to the character at the position or NULL if no character is at the position.
 */
const char *Layouter::GetCharAtPosition(int x) const
{
	const ParagraphLayouter::Line *line = *this->Begin();

	for (int run_index = 0; run_index < line->CountRuns(); run_index++) {
		const ParagraphLayouter::VisualRun *run = line->GetVisualRun(run_index);

		for (int i = 0; i < run->GetGlyphCount(); i++) {
			/* Not a valid glyph (empty). */
			if (run->GetGlyphs()[i] == 0xFFFF) continue;

			int begin_x = (int)run->GetPositions()[i * 2];
			int end_x   = (int)run->GetPositions()[i * 2 + 2];

			if (IsInsideMM(x, begin_x, end_x)) {
				/* Found our glyph, now convert to UTF-8 string index. */
				size_t index = run->GetGlyphToCharMap()[i];

				size_t cur_idx = 0;
				for (const char *str = this->string; *str != '\0'; ) {
					if (cur_idx == index) return str;

					WChar c = Utf8Consume(&str);
					cur_idx += line->GetInternalCharLength(c);
				}
			}
		}
	}

	return NULL;
}

/**
 * Get a static font instance.
 */
Font *Layouter::GetFont(FontSize size, TextColour colour)
{
	FontColourMap::iterator it = fonts[size].Find(colour);
	if (it != fonts[size].End()) return it->second;

	Font *f = Font::Create(size, colour);
	*fonts[size].Append() = FontColourMap::Pair(colour, f);
	return f;
}

/**
 * Reset cached font information.
 * @param size Font size to reset.
 */
void Layouter::ResetFontCache(FontSize size)
{
	for (FontColourMap::iterator it = fonts[size].Begin(); it != fonts[size].End(); ++it) {
		delete it->second;
	}
	fonts[size].Clear();

	/* We must reset the linecache since it references the just freed fonts */
	ResetLineCache();
}

/**
 * Get reference to cache item.
 * If the item does not exist yet, it is default constructed.
 * @param str Source string of the line (including colour and font size codes).
 * @param len Length of \a str in bytes (no termination).
 * @param state State of the font at the beginning of the line.
 * @return Reference to cache item.
 */
Layouter::LineCacheItem &Layouter::GetCachedParagraphLayout(const char *str, size_t len, const FontState &state)
{
	if (linecache == NULL) {
		/* Create linecache on first access to avoid trouble with initialisation order of static variables. */
		linecache = new LineCache();
	}

	LineCacheKey key;
	key.state_before = state;
	key.str.assign(str, len);
	return (*linecache)[key];
}

/**
 * Clear line cache.
 */
void Layouter::ResetLineCache()
{
	if (linecache != NULL) linecache->clear();
}

/**
 * Reduce the size of linecache if necessary to prevent infinite growth.
 */
void Layouter::ReduceLineCache()
{
	if (linecache != NULL) {
		/* TODO LRU cache would be fancy, but not exactly necessary */
		if (linecache->size() > 4096) ResetLineCache();
	}
}
