/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/

#include "precomp.h"

#include "OutputCell.hpp"

#pragma warning(push)
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)
#include "../interactivity/inc/ServiceLocator.hpp"
#pragma warning(pop)

#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/inc/convert.hpp"
#include "../../inc/conattrs.hpp"
#include "../../host/dbcs.h"

using namespace Microsoft::Console::Interactivity;

bool operator==(const OutputCell& a, const OutputCell& b) noexcept
{
    return (a._singleChar == b._singleChar &&
            a._charData == b._charData &&
            a._dbcsAttribute == b._dbcsAttribute &&
            a._textAttribute == b._textAttribute &&
            a._behavior == b._behavior);
}

static constexpr TextAttribute InvalidTextAttribute{ INVALID_COLOR, INVALID_COLOR };

std::vector<OutputCell> OutputCell::FromUtf16(const std::vector<std::vector<wchar_t>>& utf16Glyphs)
{
    return _fromUtf16(utf16Glyphs, { TextAttributeBehavior::Current });
}

std::vector<OutputCell> OutputCell::_fromUtf16(const std::vector<std::vector<wchar_t>>& utf16Glyphs,
                                               const std::variant<TextAttribute, TextAttributeBehavior> textAttrVariant)
{
    std::vector<OutputCell> cells;

    auto constructorDispatch = [&](const std::wstring_view glyph, const DbcsAttribute dbcsAttr)
    {
        if (textAttrVariant.index() == 0)
        {
            cells.emplace_back(glyph, dbcsAttr, std::get<0>(textAttrVariant));
        }
        else
        {
            cells.emplace_back(glyph, dbcsAttr, std::get<1>(textAttrVariant));
        }
    };

    for (const auto glyph : utf16Glyphs)
    {
        DbcsAttribute dbcsAttr;
        const std::wstring_view glyphView{ glyph.data(), glyph.size() };
        if (IsGlyphFullWidth(glyphView))
        {
            dbcsAttr.SetLeading();
            constructorDispatch(glyphView, dbcsAttr);
            dbcsAttr.SetTrailing();
        }
        constructorDispatch(glyphView, dbcsAttr);
    }
    return cells;
}

OutputCell::OutputCell(const std::wstring_view charData,
                       const DbcsAttribute dbcsAttribute,
                       const TextAttributeBehavior behavior) :
    _singleChar{ UNICODE_INVALID },
    _dbcsAttribute{ dbcsAttribute },
    _textAttribute{ InvalidTextAttribute },
    _behavior{ behavior }
{
    THROW_HR_IF(E_INVALIDARG, charData.empty());
    _setFromStringView(charData);
    _setFromBehavior(behavior);
}

OutputCell::OutputCell(const std::wstring_view charData,
                       const DbcsAttribute dbcsAttribute,
                       const TextAttribute textAttribute) :
    _singleChar{ UNICODE_INVALID },
    _dbcsAttribute{ dbcsAttribute },
    _textAttribute{ textAttribute },
    _behavior{ TextAttributeBehavior::Stored }
{
    THROW_HR_IF(E_INVALIDARG, charData.empty());
    _setFromStringView(charData);
}

OutputCell::OutputCell(const CHAR_INFO& charInfo) :
    _singleChar{ UNICODE_INVALID },
    _dbcsAttribute{},
    _textAttribute{ InvalidTextAttribute }
{
    _setFromCharInfo(charInfo);
}

OutputCell::OutputCell(const OutputCellView& cell)
{
    _setFromOutputCellView(cell);
}

const std::wstring_view OutputCell::Chars() const noexcept
{
    if (_useSingle())
    {
        return { &_singleChar, 1 };
    }
    else
    {
        return { _charData.data(), _charData.size() };
    }
}

void OutputCell::SetChars(const std::wstring_view chars)
{
    _setFromStringView(chars);
}

DbcsAttribute& OutputCell::DbcsAttr() noexcept
{
    return _dbcsAttribute;
}

TextAttribute& OutputCell::TextAttr()
{
    THROW_HR_IF(E_INVALIDARG, _behavior == TextAttributeBehavior::Current);
    return _textAttribute;
}

bool OutputCell::_useSingle() const noexcept
{
    return _charData.empty();
}

void OutputCell::_setFromBehavior(const TextAttributeBehavior behavior)
{
    THROW_HR_IF(E_INVALIDARG, behavior == TextAttributeBehavior::Stored);
    // Can we get rid of this?
    // if (behavior == TextAttributeBehavior::Default)
    // {
    //     const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    //     _textAttribute = gci.GetActiveOutputBuffer().GetAttributes();
    // }
}

void OutputCell::_setFromCharInfo(const CHAR_INFO& charInfo)
{
    _singleChar = charInfo.Char.UnicodeChar;

    if (WI_IsFlagSet(charInfo.Attributes, COMMON_LVB_LEADING_BYTE))
    {
        _dbcsAttribute.SetLeading();
    }
    else if (WI_IsFlagSet(charInfo.Attributes, COMMON_LVB_TRAILING_BYTE))
    {
        _dbcsAttribute.SetTrailing();
    }
    _textAttribute.SetFromLegacy(charInfo.Attributes);

    _behavior = TextAttributeBehavior::Stored;
}

void OutputCell::_setFromStringView(const std::wstring_view view)
{
    _singleChar = UNICODE_INVALID;

    if (view.size() == 1)
    {
        _singleChar = view.at(0);
    }
    else
    {
        _charData.assign(view.cbegin(), view.cend());
    }
}

void OutputCell::_setFromOutputCellView(const OutputCellView& cell)
{
    _dbcsAttribute = cell.DbcsAttr();
    _textAttribute = cell.TextAttr();
    _behavior = cell.TextAttrBehavior();

    const auto& view = cell.Chars();
    if (view.size() > 1)
    {
        _charData.assign(view.cbegin(), view.cend());
        _singleChar = UNICODE_INVALID;
    }
    else
    {
        _singleChar = view.front();
    }
}
