//
// VMime library (http://vmime.sourceforge.net)
// Copyright (C) 2002-2004 Vincent Richard <vincent@vincent-richard.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include "mediaType.hpp"
#include "parserHelpers.hpp"


namespace vmime
{


mediaType::mediaType()
	: m_type(mediaTypes::APPLICATION), m_subType(mediaTypes::APPLICATION_OCTET_STREAM)
{
}


mediaType::mediaType(const string& type)
{
	parse(type);
}


mediaType::mediaType(const string& type, const string& subType)
{
	set(type, subType);
}


void mediaType::parse(const string& buffer, const string::size_type position,
	const string::size_type end, string::size_type* newPosition)
{
	const string::value_type* const pend = buffer.data() + end;
	const string::value_type* const pstart = buffer.data() + position;
	const string::value_type* p = pstart;

	// Extract the type
	const string::size_type typeStart = position;

	while (p < pend && *p != '/') ++p;

	m_type = toLower(string(buffer.begin() + typeStart,
	                        buffer.begin() + position + (p - pstart)));

	if (p < pend)
	{
		// Skip '/' character
		++p;

		// Extract the sub-type
		m_subType = toLower(string(buffer.begin() + position + (p - pstart),
		                           buffer.begin() + end));
	}

	if (newPosition)
		*newPosition = end;
}


void mediaType::generate(utility::outputStream& os, const string::size_type maxLineLength,
	const string::size_type curLinePos, string::size_type* newLinePos) const
{
	const string value = toLower(m_type) + "/" + toLower(m_subType);

	if (curLinePos + value.length() > maxLineLength)
	{
		os << NEW_LINE_SEQUENCE;
		os << value;

		if (newLinePos)
			*newLinePos = NEW_LINE_SEQUENCE_LENGTH + value.length();
	}
	else
	{
		os << value;

		if (newLinePos)
			*newLinePos = curLinePos + value.length();
	}
}


const bool mediaType::operator==(const mediaType& type) const
{
	return (m_type == type.m_type && m_subType == type.m_subType);
}


const bool mediaType::operator!=(const mediaType& type) const
{
	return !(*this == type);
}


mediaType& mediaType::operator=(const mediaType& type)
{
	m_type = type.m_type;
	m_subType = type.m_subType;

	return (*this);
}


mediaType& mediaType::operator=(const string& type)
{
	parse(type);
	return (*this);
}


} // vmime
