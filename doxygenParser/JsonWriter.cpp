#include "JsonWriter.h"
#include <set>
#include <sstream>
#include <iostream>
#include <fstream>

void JsonWriter::Initialize(const std::vector<string>& namespaces, const std::vector<Class>& classes)
{
	CalculateNamespaces(namespaces);
	CalculateClasses(classes);
}

void JsonWriter::CalculateNamespaces(const std::vector<string>& namespaces)
{
	for (const auto& item: namespaces) {
		Namespace newNamespace;
		newNamespace.name = GetLastId(item);
		newNamespace.parentId = GetWithoutLastId(item);
		m_namespaces.insert(std::map<string, Namespace>::value_type(item, std::move(newNamespace)));
	}
}

std::vector<string> split(const string &s, _TCHAR delim) {
	std::vector<string> elems;
	std::basic_stringstream<_TCHAR> ss(s);
	string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

template<std::size_t Size>
std::vector<string> split(const string &s, const _TCHAR (&delim)[Size]) {
	std::vector<string> elems = split(s, delim[0]);
	for (std::size_t i = 1; i < Size; i++) {
		std::vector<string> iElems;
		for (const auto& item: elems) {
			for (const auto& elem: split(item, delim[i])) {
				iElems.push_back(elem);
			}
		}

		elems.swap(iElems);
	}
	return elems;
}

void JsonWriter::CalculateClasses(const std::vector<Class>& classes)
{
	std::set<string> ids;
	for (const auto& item: classes) {
		ids.insert(item.name);
	}

	for (const auto& item: classes) {
		ClassEntry newEntry;
		newEntry.name = GetLastId(item.name);
		newEntry.doxygenId = item.doxygenId;
		newEntry.type = item.type;
		string prefix = GetWithoutLastId(item.name);

		// newEntry.parentId
		if (ids.find(prefix) != ids.end()) {
			newEntry.parentId = prefix;
		}

		// newEntry.namespaceId
		while (!prefix.empty()) {
			if (ids.find(prefix) == ids.end() &&
				m_namespaces.find(prefix) != m_namespaces.end()) {
					newEntry.namespaceId = prefix;
					break;
			}
			prefix = GetWithoutLastId(prefix);
		}

		//newEntry.connections
		for (const auto& parent: item.inheritance) {
			for (auto& connection: GetConnections(parent.first, newEntry.namespaceId, ids, parent.second)) {
				newEntry.connections.push_back(connection);
			}
		}

		for (const auto& member: item.members) {
			for (auto& connection: GetConnections(member.type, newEntry.namespaceId, ids, member.protectionLevel)) {
				connection.type = MEMBER_ITEM;
				connection.connectionCode += _T(" ");
				connection.connectionCode += member.name;
				newEntry.connections.push_back(connection);
			}
		}


		m_classes.insert(std::map<string, ClassEntry>::value_type(item.name, std::move(newEntry)));
	}
}

std::vector<JsonWriter::ClassConnection> JsonWriter::GetConnections(const string& type, const string& namespaceId, const std::set<string>& ids, EProtectionLevel protLevel) const
{
	std::vector<ClassConnection> result;
	ClassConnection connection;
	switch (protLevel)
	{
	case PRIVATE: connection.connectionCode = _T("private "); break;
	case PROTECTED: connection.connectionCode = _T("protected "); break;
	case PUBLIC: connection.connectionCode = _T("public "); break;
	}
	connection.connectionCode += type;
	if (ids.find(type) != ids.end()) {
		connection.type = DIRECT_INHERITANCE;
		connection.targetId = type;
		result.push_back(connection);
	} else if (ids.find(namespaceId + _T("::") + type) != ids.end()) {
		connection.type = DIRECT_INHERITANCE;
		connection.targetId = namespaceId + _T("::") + type;
		result.push_back(connection);
	} else {
		for (const auto& defItem: split(type, _T(",<> \t&*"))) {
			if (ids.find(defItem) != ids.end()) {
				connection.type = INDIRECT_INHERITANCE;
				connection.targetId = defItem;
				result.push_back(connection);
			} else if (ids.find(namespaceId + _T("::") + defItem) != ids.end()) {
				connection.type = INDIRECT_INHERITANCE;
				connection.targetId = namespaceId + _T("::") + defItem;
				result.push_back(connection);
			}
		}
	}

	return result;
}

string JsonWriter::GetLastId(const string& name)
{
	const std::size_t found = name.rfind(_T("::"));
	if (found != std::string::npos) {
		return name.substr(found + 2);
	}
	return name;
}

string JsonWriter::GetWithoutLastId(const string& name)
{
	const std::size_t found = name.rfind(_T("::"));
	if (found != std::string::npos) {
		return name.substr(0, found);
	}
	return string();
}

void JsonWriter::Write()
{
	// Clear orphan items
	std::set<string> namespacesToBeCleared, classesToBeCleared;
	for (const auto& n: m_namespaces) {
		namespacesToBeCleared.insert(n.first);
	}
	for (const auto& c: m_classes) {
		classesToBeCleared.insert(c.first);
	}

	for (const auto& c: m_classes) {
		classesToBeCleared.erase(c.second.parentId);
		for (const auto& connection: c.second.connections) {		
			classesToBeCleared.erase(connection.targetId);
		}
	}
	for (const auto& c: classesToBeCleared) {
		m_classes.erase(c);
	}
	for (const auto& c: m_classes) {
		namespacesToBeCleared.erase(c.second.namespaceId);
	}

	bool change;
	do {
		change = false;
		for (const auto& n: m_namespaces) {
			if (namespacesToBeCleared.find(n.first) == namespacesToBeCleared.end())	{
				change = namespacesToBeCleared.erase(n.second.parentId) || change;
			}
		}
	} while(change);

	for (const auto& n: namespacesToBeCleared) {
		m_namespaces.erase(n);
	}


	std::basic_ofstream<_TCHAR> file(m_outputDir+_T("\\classes.json"));
	file << _T("{\"nodes\": [") << std::endl;
	{
		bool first = true;
		for (const auto& n: m_namespaces) {
			if (!first) {
				file << _T(",") << std::endl;
			}
			file << WriteNode(n.first, n.second.name, n.second.parentId, _T("namespace"));
			first = false;
		}
		for (const auto& c: m_classes) {
			if (!first) {
				file << _T(",") << std::endl;
			}
			const _TCHAR* type = nullptr;
			switch(c.second.type) {
			case Class::STRUCT: type = _T("struct"); break;
			case Class::INTERFACE: type = _T("interface"); break;
			case Class::CLASS: type = _T("class"); break;
			}
			file << WriteNode(c.first, c.second.name, c.second.namespaceId, type);			
			first = false;
		}
	}

	file << std::endl << _T("], \"edges\": [") << std::endl;
	{
		bool first = true;
		for (const auto& c: m_classes) {			
			for (const auto& connection: c.second.connections) {
				if (!first) {
					file << _T(",") << std::endl;
				}

				const _TCHAR* type = nullptr;
				switch(connection.type) {
				case DIRECT_INHERITANCE: type = _T("inherits"); break;
				case INDIRECT_INHERITANCE: type = _T("derives"); break;
				case MEMBER_ITEM: type = _T("member"); break;
				}

				file << WriteEdge(c.first, connection.targetId, connection.connectionCode, type);
				first = false;
			}

			if (!c.second.parentId.empty()) {
				if (!first) {
					file << _T(",") << std::endl;
				}
				file << WriteEdge(c.first, c.second.parentId, nullptr, _T("parent"));
				first = false;
			}
		}
	}
	file << std::endl << _T("]}") << std::endl;
	file.close();
	/*
	for (const auto& n: m_namespaces) {
		std::wcout << _T("namespace: ") << n.first << _T(", ") << n.second.name << _T(", ") << n.second.parentId << std::endl;
	}

	for (const auto& c: m_classes) {
		std::wcout << _T("class: ") << c.first << _T(", ")
			<< c.second.name << _T(", ")
			<< c.second.namespaceId << _T(", ")
			<< c.second.parentId << std::endl;
		for (const auto& connection: c.second.connections) {
			std::wcout << _T("connection: ") << connection.targetId << _T(", ")
				<< connection.type << _T(", ")
				<< connection.connectionCode << _T(", ") << std::endl;
		}
	}*/
}


string JsonWriter::WriteNode(const stringRef& id, const stringRef& name, const stringRef& parent, const stringRef& type) const
{
	std::basic_ostringstream<_TCHAR> s;
	s << _T("{\"id\":\"") << id.str()
		<< _T("\", \"name\":\"") << name.str()
		<< _T("\", \"type\":\"") << type.str();
	if (parent) {
		s << _T("\", \"parent\":\"") << parent.str();
	}
	s << _T("\"}");
	return s.str();
}

string JsonWriter::WriteEdge(const stringRef& source, const stringRef& target,  const stringRef& code, const stringRef& type) const
{
	std::basic_ostringstream<_TCHAR> s;
	s << _T("{\"source\":\"") << source.str()
		<< _T("\", \"target\":\"") << target.str()
		<< _T("\", \"code\":\"") << code.str()
		<< _T("\", \"type\":\"") << type.str()
		<< _T("\"}");
	return s.str();
}