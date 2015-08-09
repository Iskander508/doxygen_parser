#ifndef TYPES_H__
#define TYPES_H__

#include <tchar.h>
#include "../rapidxml/rapidxml.hpp"
#include <string>

typedef std::basic_string<_TCHAR> string;
typedef rapidxml::xml_node<_TCHAR> Node;
typedef rapidxml::xml_attribute<_TCHAR> Attribute;

struct stringRef {
	stringRef() : m_str(nullptr), m_owner(false) {}
	stringRef(const _TCHAR* str) : m_str(str), m_owner(false) {}
	stringRef(const stringRef& that) : m_buffer(that.m_buffer), m_owner(that.m_owner), m_str(m_owner ? m_buffer.c_str() : that.m_str) {}
	stringRef(stringRef&& that) : m_buffer(std::move(that.m_buffer)), m_str(that.m_str), m_owner(that.m_owner) {}
	stringRef(const string& that) : m_buffer(that), m_str(m_buffer.c_str()), m_owner(true) {}
	stringRef(string&& that) : m_buffer(std::move(that)), m_str(m_buffer.c_str()), m_owner(true) {}

	const _TCHAR* str() const { return m_str ? m_str : _T(""); }
	std::size_t size() const { return m_str ? string(str()).size() : 0; }
	operator bool() const { return m_str != nullptr && m_str[0] != _T('\0'); }
	bool operator >(const _TCHAR* that) const { return string(str()) > string(that); }
	bool operator >=(const _TCHAR* that) const { return string(str()) >= string(that); }
	bool operator <(const _TCHAR* that) const { return string(str()) < string(that); }
	bool operator <=(const _TCHAR* that) const { return string(str()) <= string(that); }
	bool operator ==(const _TCHAR* that) const { return string(str()) == string(that); }
	bool operator !=(const _TCHAR* that) const { return !operator==(that); }

private:
	const bool m_owner; // true if keeping the string in m_buffer
	const string m_buffer;
	const _TCHAR* const m_str;
};

#endif // TYPES_H__
