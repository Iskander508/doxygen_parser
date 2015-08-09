#include "JsonWriter.h"
#include <set>
#include <sstream>
#include <iostream>
#include <fstream>
#include <regex>

void JsonWriter::Initialize(const std::vector<string>& namespaces, const std::vector<Class>& classes)
{
	CalculateNamespaces(namespaces);
	CalculateClasses(classes);
	CalculateMethodOverrides();
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
		newEntry.data = item;
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
				connection.connectedMember = member.name;
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

void JsonWriter::ClearOrphanItems()
{
	bool erased;
	do {
		erased = false;
		std::set<string> classesToBeCleared;
		for (const auto& c: m_classes) {
			classesToBeCleared.insert(c.first);
		}

		for (const auto& c: m_classes) {
			classesToBeCleared.erase(c.second.parentId);
			for (const auto& connection: c.second.connections) {		
				classesToBeCleared.erase(connection.targetId);
				classesToBeCleared.erase(c.first);
			}
		}
		for (const auto& c: classesToBeCleared) {
			erased = m_classes.erase(c) || erased;
		}
	} while (erased);


	do {
		erased = false;
		std::set<string> namespacesToBeCleared;
		for (const auto& n: m_namespaces) {
			namespacesToBeCleared.insert(n.first);
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
			erased = m_namespaces.erase(n) || erased;
		}
	} while (erased);
}

void JsonWriter::WriteClassesJson()
{
	ClearOrphanItems();

	std::basic_ofstream<_TCHAR> file(m_outputDir + _T("\\classes.json"));
	file << _T("{\"nodes\": [") << std::endl;
	{
		bool first = true;
		for (const auto& n: m_namespaces) {
			if (!first) {
				file << _T(",") << std::endl;
			}
			file << WriteNode(n.first, n.second.name, n.first, _T("namespace"), n.second.parentId);
			first = false;
		}
		for (const auto& c: m_classes) {
			if (!first) {
				file << _T(",") << std::endl;
			}
			const _TCHAR* type = nullptr;
			switch(c.second.data.type) {
			case Class::STRUCT: type = _T("struct"); break;
			case Class::INTERFACE: type = _T("interface"); break;
			case Class::CLASS: type = _T("class"); break;
			}
			file << WriteNode(c.first, c.second.name, c.first, type, c.second.namespaceId, c.second.data.doxygenId);			
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

				file << WriteEdge(c.first, connection.targetId, type, connection.connectionCode);
				first = false;
			}

			if (!c.second.parentId.empty()) {
				if (!first) {
					file << _T(",") << std::endl;
				}
				file << WriteEdge(c.first, c.second.parentId, _T("parent"));
				first = false;
			}
		}
	}
	file << std::endl << _T("]}") << std::endl;
	file.close();
}

string escape(string s)
{
	string result = replaceAll(std::move(s), _T("\\"), _T("\\\\"));
	result = replaceAll(std::move(result), _T("\""), _T("\\\""));
	result = replaceAll(std::move(result), _T("\n"), _T("\\n"));
	result = replaceAll(std::move(result), _T("\t"), _T("\\t"));
    return result;
}

string JsonWriter::WriteNode(const stringRef& id, const stringRef& shortName, const stringRef& longName, const stringRef& type, const stringRef& parent, const stringRef& reference, const std::vector<string>& classes) const
{
	std::basic_ostringstream<_TCHAR> s;
	s	<< _T("{\"id\":\"") << escape(id.str())
		<< _T("\", \"shortName\":\"") << escape(shortName.str())
		<< _T("\", \"type\":\"") << escape(type.str());
	if (parent) {
		s << _T("\", \"parent\":\"") << escape(parent.str());
	}
	if (longName) {
		s << _T("\", \"longName\":\"") << escape(longName.str());
	}
	if (reference) {
		s << _T("\", \"reference\":\"") << escape(reference.str());
	}
	if (!classes.empty()) {
		s << _T("\", \"classes\":[");
		bool first = true;
		for (const auto& c : classes) {
			if (!first) {
				s << _T(",");
			}
			first = false;
			s << _T("\"") << escape(c) << _T("\"");
		}
		s << _T("]}");
	} else s << _T("\"}");
	return s.str();
}

string JsonWriter::WriteEdge(const stringRef& source, const stringRef& target, const stringRef& type, const stringRef& description) const
{
	std::basic_ostringstream<_TCHAR> s;
	s	<< _T("{\"source\":\"") << escape(source.str())
		<< _T("\", \"target\":\"") << escape(target.str())
		<< _T("\", \"type\":\"") << escape(type.str());
	if (description) {
		s << _T("\", \"description\":\"") << escape(description.str());
	}
	s << _T("\"}");
	return s.str();
}

void JsonWriter::CalculateMethodOverrides()
{
	for (auto& c: m_classes) {
		for (auto& method: c.second.data.methods) {
			if (!method.Virtual) continue;

			std::vector<ClassConnection> connections;
			for (auto& connection: c.second.connections) {
				if (connection.type != DIRECT_INHERITANCE) continue;
				connections.push_back(connection);
			}
			for (auto& connection: c.second.connections) {
				if (connection.type != INDIRECT_INHERITANCE) continue;
				connections.push_back(connection);
			}

			bool found = false;
			for (auto& connection: connections) {
				if (found) break;
				const auto it = m_classes.find(connection.targetId);
				if (it == m_classes.end()) continue;

				for (auto& targetMethod: it->second.data.methods) {
					if (!targetMethod.Virtual) continue;

					if (targetMethod.name == method.name && targetMethod.Const == method.Const && targetMethod.params.size() == method.params.size()) {
						c.second.methodOverrides.insert(std::map<string, string>::value_type(method.doxygenId, it->first));
						found = true;
						break;
					}
				}
			}
		}
	}
}

void JsonWriter::ProcessFileDef(const Element& fileDef)
{
	const stringRef location = fileDef.GetElement(_T("location")).GetAttribute(_T("file")).str();
	for (auto& c: m_classes) {
		for (auto& method: c.second.data.methods) {
			if (method.locationFile != location.str()) continue;

			bool started = false;
			for (const auto& line: fileDef.GetElement(_T("programlisting")).Elements(_T("codeline"))) {
				const stringRef lineNo = line.GetAttribute(_T("lineno"));
				bool firstLine = false;
				if (!started) {
					started = (method.bodyBeginLine == lineNo.str());
					if (!started) continue;
					firstLine = true;
				}

				MemberUsage usage;
				usage.sourceMethodId = method.doxygenId;
				usage.connectionCode = string(location.str()) + _T("(") + lineNo.str() + _T("):\n") + trim(line.Text().str());

				// remove all comments
				string text;
				for (const auto& item: line.Elements(_T("highlight"))) {
					if (item.GetAttribute(_T("class")) != _T("comment")) {
						text += item.Text().str();
					}
				}

				// remove string literal constants
				while(true) {
					const std::size_t startPosition = text.find(_T('\"'));
					if (startPosition == string::npos) break;

					std::size_t endPosition = startPosition;
					do {
						endPosition = text.find(_T('\"'), endPosition + 1);
					} while(endPosition != string::npos && text[endPosition-1] == _T('\\'));
					if (endPosition == string::npos) break;
					text.erase(startPosition, endPosition - startPosition);
				}

				// start from { if on first line
				if (firstLine) {
					const std::size_t startPosition = text.find(_T('{'));
					if (startPosition != string::npos) {
						text.erase(0, startPosition);
					} else {
						text.clear();
					}
				}
				

				// members
				for (const auto& member: c.second.data.members) {
					if (text.find(member.name) != string::npos) {
						usage.targetId = member.name;
						usage.type = MEMBER_ACCESS;
						c.second.memberUsages.push_back(usage);
					}
				}

				// methods
				for (const auto& m: c.second.data.methods) {
					if (method.Const && !m.Const) continue;

					const std::basic_regex<_TCHAR> regex((string(_T(".*[^\\.>]\\s*(\\s|[^\\w\\.>])")) + m.name + _T("\\s*\\(.*")).c_str());
					if (std::regex_match(text, regex)) {
						usage.targetId = m.doxygenId;
						usage.type = METHOD_CALL;
						c.second.memberUsages.push_back(usage);
					}
				}

				if (method.bodyEndLine == lineNo.str()) break;
			}
		}
	}
}

const _TCHAR* GetProtectionLevel(EProtectionLevel level) {
	const _TCHAR* type = nullptr;
	switch(level) {
	case PRIVATE: type = _T("private"); break;
	case PUBLIC: type = _T("public"); break;
	case PROTECTED: type = _T("protected"); break;
	case PACKAGE: type = _T("package"); break;
	}
	return type;
}

void JsonWriter::WriteSingleClassJsons()
{
	for (const auto& c: m_classes) {
		std::basic_ofstream<_TCHAR> file(m_outputDir + _T("\\") + c.second.data.doxygenId + _T(".json"));
		file << _T("{\"nodes\": [") << std::endl;
		std::set<string> collaborators;
		{
			file << WriteNode(_T("class"), c.first, c.first, _T("object"));
			if (!c.second.parentId.empty()) {
				file << _T(",") << std::endl;
				file << WriteNode(c.second.parentId, m_classes[c.second.parentId].name, c.second.parentId, _T("parent"), nullptr, m_classes[c.second.parentId].data.doxygenId);
			}
			for (const auto& connection: c.second.connections) {
				file << _T(",") << std::endl;
				file << WriteNode(connection.targetId, m_classes[connection.targetId].name, connection.targetId, _T("connection"), nullptr, m_classes[connection.targetId].data.doxygenId);
			}
			for (const auto& method: c.second.data.methods) {
				file << _T(",") << std::endl;
				std::basic_ostringstream<_TCHAR> longName; 
				longName << string(GetProtectionLevel(method.protectionLevel)) << _T(" ")
					<< (method.Virtual ? _T("virtual ") : _T(""))
					<< (method.returnType.empty() ? _T("") : method.returnType + _T(" "))
					<< method.name << _T("(");
				bool firstParam = true;
				for (const auto& param: method.params) {
					if (!firstParam) {
						longName << _T(", ");
					}
					firstParam = false;
					longName << param.type << _T(" ") << param.name;
				}
				longName << _T(")");
				if (method.Const) longName << _T(" const");
				
				std::vector<string> classes;
				classes.push_back(string(GetProtectionLevel(method.protectionLevel)));
				if (method.name == c.second.name) {
					classes.push_back(_T("constructor"));
				} else if (method.name[0] == _T('~')) {
					classes.push_back(_T("destructor"));
				} else if (method.name.find(_T("operator")) != string::npos) {
					classes.push_back(_T("operator"));
				}

				file << WriteNode(method.doxygenId, method.name, longName.str(), _T("method"), _T("class"), nullptr, classes);
			}
			for (const auto& member: c.second.data.members) {
				file << _T(",") << std::endl;
				std::basic_ostringstream<_TCHAR> longName;
				longName << string(GetProtectionLevel(member.protectionLevel)) << _T(" ")
					<< member.type << _T(" ") << member.name;

				std::vector<string> classes;
				classes.push_back(string(GetProtectionLevel(member.protectionLevel)));
				file << WriteNode(member.name, member.name, longName.str(), _T("member"), _T("class"), nullptr, classes);
			}

			// connections from other classes
			for (const auto& other: m_classes) {
				if (other.second.parentId == c.first) {
					collaborators.insert(other.first);
				}
				for (const auto& connection: other.second.connections) {
					if (connection.targetId == c.first)	{
						collaborators.insert(other.first);
					}
				}
			}
			for (const auto& collaborator: collaborators) {
				file << _T(",") << std::endl;
				file << WriteNode(collaborator, m_classes[collaborator].name, collaborator, _T("collaborator"), nullptr, m_classes[collaborator].data.doxygenId);
			}
		}

		file << std::endl << _T("], \"edges\": [") << std::endl;
		{
			bool first = true;
			if (!c.second.parentId.empty()) {
				first = false;
				file << WriteEdge(_T("class"), c.second.parentId, _T("parent"));
			}

			for (const auto& method: c.second.methodOverrides) {
				if (!first) {
					file << _T(",") << std::endl;
				}
				file << WriteEdge(method.first, method.second, _T("override"));
				first = false;
			}

			for (const auto& usage: c.second.memberUsages) {
				if (!first) {
					file << _T(",") << std::endl;
				}

				const _TCHAR* type = nullptr;
				switch(usage.type) {
				case MEMBER_ACCESS: type = _T("access"); break;
				case METHOD_CALL: type = _T("call"); break;
				}
				file << WriteEdge(usage.sourceMethodId, usage.targetId, type, usage.connectionCode);
				first = false;
			}

			for (const auto& connection: c.second.connections) {
				
				if (!first) {
					file << _T(",") << std::endl;
				}

				if (connection.type == MEMBER_ITEM) {
					file << WriteEdge(connection.connectedMember, connection.targetId, _T("member"), connection.connectionCode);
				} else {
					file << WriteEdge(_T("class"), connection.targetId, _T("inherits"), connection.connectionCode);
				}
				
				first = false;
			}

			for (const auto& collaborator: collaborators) {
				if (m_classes[collaborator].parentId == c.first) {
					if (!first) {
						file << _T(",") << std::endl;
					}
					file << WriteEdge(collaborator, _T("class"), _T("parent"));
					first = false;
				}
				for (const auto& connection: m_classes[collaborator].connections) {
					if (connection.targetId == c.first)	{
						if (!first) {
							file << _T(",") << std::endl;
						}

						if (connection.type == MEMBER_ITEM) {
							file << WriteEdge(collaborator, _T("class"), _T("member"), connection.connectionCode);
						} else {
							file << WriteEdge(collaborator, _T("class"), _T("inherits"), connection.connectionCode);
						}

						first = false;
					}
				}
			}
		}
		file << std::endl << _T("], \"class\":\"") << escape(c.first) << _T("\"}") << std::endl;
		file.close();
	}
}