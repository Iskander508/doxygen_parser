#include "ClassManager.h"
#include "JsonWriter.h"
#include <set>
#include <sstream>
#include <iostream>
#include <regex>



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


void ClassManager::Initialize()
{
	CalculateNamespaces(initNamespaces);
	CalculateClasses(initClasses);
	CalculateMethods();
}

void ClassManager::CalculateNamespaces(const std::vector<string>& namespaces)
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

void ClassManager::CalculateClasses(const std::vector<Class>& classes)
{
	std::set<string> ids;
	for (const auto& item: classes) {
		ids.insert(item.name);
	}

	std::vector<string> utilityClasses;

	for (const auto& item: classes) {
		ClassEntry newEntry;
		newEntry.name = GetLastId(item.name);
		newEntry.data = item;
		string prefix = GetWithoutLastId(item.name);

		// newEntry.parentId
		if (ids.find(prefix) != ids.end()) {
			newEntry.parentId = prefix;

			// all member classes are treated as utility classes
			newEntry.utility = true;
		}

		/*// all templated classes are treated as utility classes
		if (item.templated)	{
			newEntry.utility = true;
		}*/

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
			for (auto& connection: GetConnections(parent.classId, newEntry.namespaceId, ids, parent.protLevel, parent.Virtual)) {
				newEntry.connections.push_back(connection);

				// all directly inherited classes are treated as utility classes
				if (connection.type == DIRECT_INHERITANCE) {
					utilityClasses.push_back(connection.targetId);
				}
			}
		}

		for (const auto& member: item.members) {
			for (auto& connection: GetConnections(member.type, newEntry.namespaceId, ids, member.protectionLevel)) {
				
				// all classes directly (not as template parameter) as members are treated as utility classes
				if (connection.type == DIRECT_INHERITANCE && member.type != newEntry.parentId) {
					utilityClasses.push_back(member.type);
				}

				connection.type = MEMBER_ITEM;
				connection.connectionCode += _T(" ");
				connection.connectionCode += member.name;
				connection.connectedMember = member.name;
				newEntry.connections.push_back(connection);
			}
		}


		m_classes.insert(std::map<string, ClassEntry>::value_type(item.name, std::move(newEntry)));
	}

	for (const auto& id: utilityClasses) {
		if (m_classes.find(id) != m_classes.end()) {
			m_classes[id].utility = true;
		}
	}

	for (auto& c: m_classes) {
		std::vector<ClassConnection> newConnections;
		for (auto& connection: c.second.connections) {
			if (connection.type == DIRECT_INHERITANCE) {
				string parentId;
				{
					string tmpCurClass = connection.targetId;
					do {
						const auto it = m_classes.find(tmpCurClass);
						if (it != m_classes.end()) {
							if (tmpCurClass != connection.targetId) {
								parentId = tmpCurClass;
							}
							tmpCurClass = it->second.parentId;
						}
					} while(!tmpCurClass.empty());
				}				

				if (!parentId.empty() && !m_classes[parentId].utility && !m_classes[parentId].data.interface) {
					ClassConnection indirectConnection = connection;
					indirectConnection.type = INDIRECT_INHERITANCE;
					indirectConnection.targetId = parentId;
					newConnections.push_back(std::move(indirectConnection));
				}
			}
		}
		c.second.connections.insert(c.second.connections.end(), newConnections.begin(), newConnections.end());
	}
}

std::vector<ClassManager::ClassConnection> ClassManager::GetConnections(const string& type, const string& namespaceId, const std::set<string>& ids, EProtectionLevel protLevel, bool Virtual) const
{
	std::vector<ClassConnection> result;
	ClassConnection connection;
	connection.Virtual = Virtual;
	connection.protectionLevel = protLevel;

	switch (protLevel)
	{
	case PRIVATE: connection.connectionCode = _T("private "); break;
	case PROTECTED: connection.connectionCode = _T("protected "); break;
	case PUBLIC: connection.connectionCode = _T("public "); break;
	}
	if (Virtual) {
		connection.connectionCode = _T("virtual ");
	}
	connection.connectionCode += type;

	const std::size_t templateDelimiter = type.find_first_of(_T('<'));
	string directType = (templateDelimiter == string::npos) ? type : type.substr(0, templateDelimiter);
	string indirectTypes = (templateDelimiter == string::npos) ? string() : type.substr(templateDelimiter + 1);

	connection.type = DIRECT_INHERITANCE;
	for (const auto& defItem: split(directType, _T(" \t&*"))) {
		if (ids.find(defItem) != ids.end()) {
			connection.targetId = defItem;
			result.push_back(connection);
		} else {
			string namespaceStr = namespaceId;
			while(!namespaceStr.empty()) {
				if (ids.find(namespaceStr + _T("::") + defItem) != ids.end()) {
					connection.targetId = namespaceStr + _T("::") + defItem;
					result.push_back(connection);
					break;
				}

				const std::size_t pos = namespaceStr.rfind(_T("::"));
				if (pos == string::npos) break;
				namespaceStr = namespaceStr.substr(0, pos);
			}
		}
	}

	connection.type = INDIRECT_INHERITANCE;
	for (const auto& defItem: split(indirectTypes, _T(",<> \t&*"))) {
		if (ids.find(defItem) != ids.end()) {
			connection.targetId = defItem;
			result.push_back(connection);
		} else {
			string namespaceStr = namespaceId;
			while(!namespaceStr.empty()) {
				if (ids.find(namespaceStr + _T("::") + defItem) != ids.end()) {
					connection.targetId = namespaceStr + _T("::") + defItem;
					result.push_back(connection);
					break;
				}

				const std::size_t pos = namespaceStr.rfind(_T("::"));
				if (pos == string::npos) break;
				namespaceStr = namespaceStr.substr(0, pos);
			}
		}

	}

	return result;
}

string ClassManager::GetLastId(const string& name)
{
	const std::size_t found = name.rfind(_T("::"));
	if (found != std::string::npos) {
		return name.substr(found + 2);
	}
	return name;
}

string ClassManager::GetWithoutLastId(const string& name)
{
	const std::size_t found = name.rfind(_T("::"));
	if (found != std::string::npos) {
		return name.substr(0, found);
	}
	return string();
}

void ClassManager::ClearOrphanItems()
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

string GetNamespaceFileName(const stringRef& namespaceId, bool external = true)
{
	return string(_T("namespace_")) + (external ? _T("external_") : _T("internal_")) + replaceAll(namespaceId.str(), _T("::"), _T("_"));
}

void ClassManager::WriteClassesJson()
{
	ClearOrphanItems();

	JsonWriter file(m_outputDir + _T("\\classes.json"));
	for (const auto& n: m_namespaces) {
		file.WriteNode(n.first, n.second.name, n.first, nullptr, _T("namespace"), n.second.parentId, GetNamespaceFileName(n.first));
	}
	for (auto& c: m_classes) {

		const _TCHAR* type = nullptr;
		switch(c.second.data.type) {
		case Class::STRUCT: type = _T("struct"); break;
		case Class::CLASS: type = _T("class"); break;
		}
		if (c.second.data.interface) {
			c.second.utility = false; // interfaces are not utilities
			type = _T("interface");
		}

		// strip the utility classes
		if (c.second.utility) continue;

		file.WriteNode(c.first, c.second.name, c.first, c.first, type, c.second.namespaceId, c.second.data.doxygenId, c.second.data.filename, c.second.data.description);			
	}

	for (const auto& c: m_classes) {			
		for (const auto& connection: c.second.connections) {

			const _TCHAR* type = nullptr;
			switch (connection.type)
			{
			case MEMBER_ITEM: type = _T("member"); break;
			default: type = _T("derives"); break;
			}

			std::vector<string> classes;
			switch (connection.type)
			{
			case DIRECT_INHERITANCE: classes.push_back(_T("direct")); break;
			case INDIRECT_INHERITANCE: classes.push_back(_T("indirect")); break;
			}
			if (connection.Virtual)	{
				classes.push_back(_T("virtual"));
			}
			classes.push_back(GetProtectionLevel(connection.protectionLevel));

			file.WriteEdge(c.first, connection.targetId, type, connection.connectionCode, classes);
		}

		if (!c.second.parentId.empty()) {
			file.WriteEdge(c.first, c.second.parentId, _T("parent"));
		}
	}

	file.ClearOrphans();
}

void ClassManager::CalculateMethods()
{
	for (auto& c: m_classes) {
		const std::map<string, string> usableClasses = GetUsableClasses(c.first, c.second.namespaceId);
		for (auto& method: c.second.data.methods) {
			// Get Other types usages in return values & parameters
			for (const auto& usable: usableClasses) {
				const std::basic_regex<_TCHAR> regex((string(_T("(^|.*[^\\w])")) + usable.first + _T("($|[^\\w:].*)")).c_str());
				if (std::regex_match(method.returnType, regex)) {
					MemberUsage usage;
					usage.sourceMethodId = method.doxygenId;
					usage.connectionCode = string(_T("return type: ") + method.returnType);
					usage.targetId = usable.second;
					usage.type = CLASS_USAGE;
					c.second.memberUsages.push_back(std::move(usage));
					if (method.protectionLevel != PRIVATE) {
						m_classes[usable.second].utility = true;
					}
				}

				for (const auto& param: method.params) {
					if (std::regex_match(param.type, regex)) {
						MemberUsage usage;
						usage.sourceMethodId = method.doxygenId;
						usage.connectionCode = string(_T("param: ") + param.type + _T(" ") + param.name);
						usage.targetId = usable.second;
						usage.type = CLASS_USAGE;
						c.second.memberUsages.push_back(std::move(usage));
						if (method.protectionLevel != PRIVATE) {
							m_classes[usable.second].utility = true;
						}
					}
				}
			}
			

			// Get overrides
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

		// Find utility classes used as members of other classes
		for (auto& member: c.second.data.members) {
			for (const auto& usable: usableClasses) {
				const std::basic_regex<_TCHAR> regex((string(_T("(^|.*[^\\w])")) + usable.first + _T("($|[^\\w:].*)")).c_str());
				if (std::regex_match(member.type, regex) && usable.second != c.second.parentId) {
					m_classes[usable.second].utility = true;
				}
			}
		}
	}
}

void ClassManager::ProcessDef(const Element& classDef)
{
	const stringRef kind = classDef.GetAttribute(_T("kind"));

	if (kind == _T("namespace")) {
		initNamespaces.push_back(string(classDef.GetElement(_T("compoundname")).Text().str()));
	} else if (kind == _T("class") || kind == _T("struct")) {
		Class newClass;
		newClass.doxygenId = classDef.GetAttribute(_T("id")).str();
		newClass.type = kind == _T("class") ? Class::CLASS : Class::STRUCT;

		newClass.name = classDef.GetElement(_T("compoundname")).Text().str();
		std::wcout << _T("New class: ") << newClass.name << std::endl;

		newClass.filename = classDef.GetElement(_T("location")).GetAttribute(_T("file")).str();
		newClass.templated = classDef.GetElement(_T("templateparamlist"));
		newClass.description = trim(classDef.GetElement(_T("briefdescription")).Text().str());

		for (const auto& parent : classDef.Elements(_T("basecompoundref"))) {
			Inheritance inheritance;
			inheritance.classId = parent.Text().str();

			inheritance.protLevel = PACKAGE;
			stringRef prot = parent.GetAttribute(_T("prot"));
			if (prot == _T("public")) {
				inheritance.protLevel = PUBLIC;
			} else if (prot == _T("protected")) {
				inheritance.protLevel = PROTECTED;
			} else if (prot == _T("private")) {
				inheritance.protLevel = PRIVATE;
			}

			inheritance.Virtual = (parent.GetAttribute(_T("virt")) == _T("virtual"));

			newClass.inheritance.push_back(std::move(inheritance));
		}


		for (const auto& section : classDef.Elements(_T("sectiondef"))) {
			for (const auto& member : section.Elements(_T("memberdef"))) {
				EProtectionLevel protectionLevel = PACKAGE;
				const stringRef prot = member.GetAttribute(_T("prot"));

				if (prot == _T("public")) {
					protectionLevel = PUBLIC;
				} else if (prot == _T("protected")) {
					protectionLevel = PROTECTED;
				} else if (prot == _T("private")) {
					protectionLevel = PRIVATE;
				}


				const stringRef kind = member.GetAttribute(_T("kind"));
				if (kind == _T("function")) {
					Method method;
					method.name = member.GetElement(_T("name")).Text().str();
					method.doxygenId = member.GetAttribute(_T("id")).str();
					method.description = trim(member.GetElement(_T("briefdescription")).Text().str());
					method.protectionLevel = protectionLevel;
					method.returnType = member.GetElement(_T("type")).Text().str();
					method.Const = member.GetAttribute(_T("const")) == _T("yes");
					method.Virtual = member.GetAttribute(_T("virt")) != _T("non-virtual");
					method.Override = (string(member.GetAttribute(_T("argsstring")).str()).find(_T("override")) != string::npos);
					for (const auto& param : member.Elements(_T("param"))) {
						Method::Param p;
						p.name = param.GetElement(_T("declname")).Text().str();
						p.type = param.GetElement(_T("type")).Text().str();
						method.params.push_back(std::move(p));
					}
					const Element location = member.GetElement(_T("location"));
					method.locationFile = location.GetAttribute(_T("bodyfile")).str();
					method.bodyBeginLine = location.GetAttribute(_T("bodystart")).str();
					method.bodyEndLine = location.GetAttribute(_T("bodyend")).str();

					if (!newClass.interface && method.Virtual && method.protectionLevel == PUBLIC && classDef.GetAttribute(_T("abstract")) == _T("yes")) {
						newClass.interface = true;
					}

					newClass.methods.push_back(std::move(method));
				} else if (kind == _T("variable")) {
					Member m;
					m.protectionLevel = protectionLevel;
					m.name = member.GetElement(_T("name")).Text().str();
					m.type = member.GetElement(_T("type")).Text().str();
					m.description = trim(member.GetElement(_T("briefdescription")).Text().str());
					newClass.members.push_back(std::move(m));}
			}
		}

		initClasses.push_back(std::move(newClass));
	}
}

std::map<string, string> ClassManager::GetUsableClasses(const stringRef& classId, const stringRef& namespaceId) const
{
	std::map<string, string> usableClasses; // search string -> class id
	for (auto& cl: m_classes) {
		if (cl.first == classId.str()) continue;
		if (cl.second.namespaceId == namespaceId.str()) {
			usableClasses.insert(std::map<string, string>::value_type(cl.second.name, cl.first));
		} else {
			string otherId = cl.first;
			for (const auto& part: split(classId.str(), _T("::"))) {
				if (otherId.substr(0, part.size() + 2) == (part + _T("::"))) {
					otherId = otherId.substr(part.size() + 2);
				} else break;
			}
			usableClasses.insert(std::map<string, string>::value_type(otherId, cl.first));
		}
	}

	return usableClasses;
}

void ClassManager::ProcessFileDef(const Element& fileDef)
{
	const stringRef location = fileDef.GetElement(_T("location")).GetAttribute(_T("file")).str();
	for (auto& c: m_classes) {

		 // search string -> class id
		const std::map<string, string> usableClasses = GetUsableClasses(c.first, c.second.namespaceId);

		for (const auto& method: c.second.data.methods) {
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
					text.erase(startPosition, endPosition - startPosition + 1);
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
					const std::basic_regex<_TCHAR> regex((string(_T(".*[^\\.>]\\s*(\\s|[^\\w\\.>])")) + member.name + _T("[^\\w].*")).c_str());
					if (std::regex_match(text, regex)) {
						usage.targetId = member.name;
						usage.type = MEMBER_ACCESS;
						std::lock_guard<std::mutex> guard(m_lock);
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
						for (const auto& o: c.second.data.methods) {
							if (o != m && o.name == m.name) {
								usage.certain = false;
								break;
							}
						}
						std::lock_guard<std::mutex> guard(m_lock);
						c.second.memberUsages.push_back(usage);
					}
				}

				// other classes usages
				for (const auto& usable: usableClasses) {
					const std::basic_regex<_TCHAR> regex((string(_T(".*[^\\.>]\\s*(\\s|[^\\w\\.>])")) + usable.first + _T("[^\\w:].*")).c_str());
					if (std::regex_match(text, regex)) {
						usage.targetId = usable.second;
						usage.type = CLASS_USAGE;
						std::lock_guard<std::mutex> guard(m_lock);
						c.second.memberUsages.push_back(usage);
					}
				}

				if (method.bodyEndLine == lineNo.str()) break;
			}
		}
	}
}

void ClassManager::WriteSingleClassJsons() const
{
	for (const auto& c: m_classes) {
		WriteSingleClassJson(c.first);
	}
}

void ClassManager::WriteNamespaceJsons() const
{
	for (const auto& n: m_namespaces) {
		WriteNamespaceJson(n.first, true);
		WriteNamespaceJson(n.first, false);
	}
}

void ClassManager::WriteSingleClassJson(const stringRef& id) const
{
	const auto& c = m_classes.at(id.str());
	JsonWriter file(m_outputDir + _T("\\") + c.data.doxygenId + _T(".json"), id);

	std::set<string> collaborators;
	file.WriteNode(_T("class"), id, id, nullptr, _T("object"), nullptr, nullptr, c.data.filename);
	if (!c.parentId.empty()) {
		file.WriteNode(c.parentId, m_classes.at(c.parentId).name, c.parentId, c.parentId, _T("parent"), nullptr, m_classes.at(c.parentId).data.doxygenId, m_classes.at(c.parentId).data.filename, m_classes.at(c.parentId).data.description);
	}
	for (const auto& connection: c.connections) {
		const _TCHAR* type = nullptr;
		switch(m_classes.at(connection.targetId).data.type) {
		case Class::CLASS: type = _T("class"); break;
		case Class::STRUCT: type = _T("struct"); break;
		}
		if (m_classes.at(connection.targetId).data.interface) {
			type = _T("interface");
		}

		std::vector<string> classes;
		if (m_classes.at(connection.targetId).utility) {
			classes.push_back(_T("utility"));
		}
		file.WriteNode(connection.targetId, m_classes.at(connection.targetId).name, connection.targetId, connection.targetId, type, nullptr, m_classes.at(connection.targetId).data.doxygenId, m_classes.at(connection.targetId).data.filename, m_classes.at(connection.targetId).data.description, classes);
	}
	for (const auto& method: c.data.methods) {
		std::basic_ostringstream<_TCHAR> hoverName; 
		hoverName << GetProtectionLevel(method.protectionLevel) << _T(" ")
			<< (method.Virtual ? _T("virtual ") : _T(""))
			<< (method.returnType.empty() ? _T("") : method.returnType + _T(" "))
			<< method.name << _T("(");
		{
			bool firstParam = true;
			for (const auto& param: method.params) {
				if (!firstParam) {
					hoverName << _T(", ");
				}
				firstParam = false;
				hoverName << param.type << _T(" ") << param.name;
			}
		}
		hoverName << _T(")");
		if (method.Const) hoverName << _T(" const");
		if (method.Override) hoverName << _T(" override");

		std::vector<string> classes;
		classes.push_back(GetProtectionLevel(method.protectionLevel));
		if (method.name == c.name) {
			classes.push_back(_T("constructor"));
		} else if (method.name[0] == _T('~')) {
			classes.push_back(_T("destructor"));
		} else if (method.name.find(_T("operator")) != string::npos) {
			classes.push_back(_T("operator"));
		}
		if (method.Const) {
			classes.push_back(_T("const"));
		}
		if (method.Virtual) {
			classes.push_back(_T("virtual"));
		}
		if (method.Override) {
			classes.push_back(_T("override"));
		}

		file.WriteNode(method.doxygenId, method.name, method.name, hoverName.str(), _T("method"), _T("class"), nullptr, nullptr, method.description, classes);
	}
	for (const auto& member: c.data.members) {
		std::basic_ostringstream<_TCHAR> longName;
		longName << GetProtectionLevel(member.protectionLevel) << _T(" ")
			<< member.type << _T(" ") << member.name;

		std::vector<string> classes;
		classes.push_back(GetProtectionLevel(member.protectionLevel));
		file.WriteNode(member.name, member.name, member.name, longName.str(), _T("member"), _T("class"), nullptr, nullptr, member.description, classes);
	}

	// connections from other classes
	for (const auto& other: m_classes) {
		if (other.second.parentId == id.str()) {
			collaborators.insert(other.first);
		}
		for (const auto& connection: other.second.connections) {
			if (connection.targetId == id.str())	{
				collaborators.insert(other.first);
			}
		}
	}
	for (const auto& usage: c.memberUsages) {
		if (usage.type != CLASS_USAGE || m_classes.find(usage.targetId) == m_classes.end()) continue;
		collaborators.insert(usage.targetId);
	}
	for (const auto& collaborator: collaborators) {
		const _TCHAR* type = nullptr;
		switch(m_classes.at(collaborator).data.type) {
		case Class::CLASS: type = _T("class"); break;
		case Class::STRUCT: type = _T("struct"); break;
		}
		if (m_classes.at(collaborator).data.interface) {
			type = _T("interface");
		}

		std::vector<string> classes;
		if (m_classes.at(collaborator).utility) {
			classes.push_back(_T("utility"));
		}
		file.WriteNode(collaborator, m_classes.at(collaborator).name, collaborator, collaborator, type, nullptr, m_classes.at(collaborator).data.doxygenId, m_classes.at(collaborator).data.filename, m_classes.at(collaborator).data.description, classes);
	}



	if (!c.parentId.empty()) {
		file.WriteEdge(_T("class"), c.parentId, _T("parent"));
	}

	for (const auto& method: c.methodOverrides) {
		file.WriteEdge(method.first, method.second, _T("override"));
	}

	for (const auto& usage: c.memberUsages) {
		if (usage.type == CLASS_USAGE && m_classes.find(usage.targetId) == m_classes.end()) continue;

		const _TCHAR* type = nullptr;
		switch(usage.type) {
		case MEMBER_ACCESS: type = _T("access"); break;
		case METHOD_CALL: type = _T("call"); break;
		case CLASS_USAGE: type = _T("use"); break;
		}

		std::vector<string> classes;
		if (!usage.certain) {
			classes.push_back(_T("uncertain"));
		}
		file.WriteEdge(usage.sourceMethodId, usage.targetId, type, usage.connectionCode, classes);
	}

	for (const auto& connection: c.connections) {

		if (connection.type == MEMBER_ITEM) {
			file.WriteEdge(connection.connectedMember, connection.targetId, _T("member"), connection.connectionCode);
		} else {
			std::vector<string> classes;
			switch (connection.type)
			{
			case DIRECT_INHERITANCE: classes.push_back(_T("direct")); break;
			case INDIRECT_INHERITANCE: classes.push_back(_T("indirect")); break;
			}
			if (connection.Virtual)	{
				classes.push_back(_T("virtual"));
			}
			classes.push_back(GetProtectionLevel(connection.protectionLevel));
			file.WriteEdge(_T("class"), connection.targetId, _T("derives"), connection.connectionCode, classes);
		}
	}

	for (const auto& collaborator: collaborators) {
		if (m_classes.at(collaborator).parentId == id.str()) {
			file.WriteEdge(collaborator, _T("class"), _T("parent"));
		}
		for (const auto& connection: m_classes.at(collaborator).connections) {
			if (connection.targetId == id.str())	{

				const _TCHAR* type = nullptr;
				switch (connection.type)
				{
				case MEMBER_ITEM: type = _T("member"); break;
				default: type = _T("derives"); break;
				}

				std::vector<string> classes;
				switch (connection.type)
				{
				case DIRECT_INHERITANCE: classes.push_back(_T("direct")); break;
				case INDIRECT_INHERITANCE: classes.push_back(_T("indirect")); break;
				}
				if (connection.Virtual)	{
					classes.push_back(_T("virtual"));
				}
				classes.push_back(GetProtectionLevel(connection.protectionLevel));

				file.WriteEdge(collaborator, _T("class"), type, connection.connectionCode, classes);
			}
		}
	}

}

void ClassManager::WriteNamespaceJson(const stringRef& namespaceId, bool external) const
{
	JsonWriter file(m_outputDir + _T("\\") + GetNamespaceFileName(namespaceId, external) + _T(".json"));
	
	std::set<string> namespaces;
	namespaces.insert(namespaceId.str());
	{
		bool newAdded = true;
		while(newAdded) {
			newAdded = false;
			for (const auto& n: m_namespaces) {
				if (!namespaces.count(n.first) && namespaces.count(n.second.parentId)) {
					namespaces.insert(n.first);
					newAdded = true;
				}
			}
		}
	}


	std::set<string> insideClasses;
	std::set<string> outsideClasses;
	for (const auto& c: m_classes) {
		if (namespaces.count(c.second.namespaceId)) {
			insideClasses.insert(c.first);
			for (const auto& connection: c.second.connections) {
				outsideClasses.insert(connection.targetId);
			}
		}
	}
	for (const auto& c: m_classes) {
		for (const auto& connection: c.second.connections) {
			if (insideClasses.count(connection.targetId)) {
				outsideClasses.insert(c.first);
				namespaces.insert(c.second.namespaceId);
			}
		}
	}


	for (const auto& c: m_classes) {
		if (!insideClasses.count(c.first) && (!external || !outsideClasses.count(c.first))) {
			continue;
		}

		// strip the utility classes
		if (c.second.utility) continue;

		const _TCHAR* type = nullptr;
		switch(c.second.data.type) {
		case Class::STRUCT: type = _T("struct"); break;
		case Class::CLASS: type = _T("class"); break;
		}
		if (c.second.data.interface) {
			type = _T("interface");
		}
		file.WriteNode(c.first, c.second.name, c.first, c.first, type, c.second.namespaceId, c.second.data.doxygenId, c.second.data.filename, c.second.data.description);

		for (const auto& connection: c.second.connections) {

			const _TCHAR* type = nullptr;
			switch (connection.type)
			{
			case MEMBER_ITEM: type = _T("member"); break;
			default: type = _T("derives"); break;
			}

			std::vector<string> classes;
			switch (connection.type)
			{
			case DIRECT_INHERITANCE: classes.push_back(_T("direct")); break;
			case INDIRECT_INHERITANCE: classes.push_back(_T("indirect")); break;
			}
			if (connection.Virtual)	{
				classes.push_back(_T("virtual"));
			}
			classes.push_back(GetProtectionLevel(connection.protectionLevel));

			file.WriteEdge(c.first, connection.targetId, type, connection.connectionCode, classes);
		}

		if (!c.second.parentId.empty()) {
			file.WriteEdge(c.first, c.second.parentId, _T("parent"));
		}
	}

	for (auto& n: namespaces) {
		if (m_namespaces.count(n)) {
			file.WriteNode(n, m_namespaces.at(n).name, n, nullptr, _T("namespace"), m_namespaces.at(n).parentId, !external && namespaceId == n.c_str() ? _T("") : GetNamespaceFileName(n, namespaceId != n.c_str()));
		}
	}

	file.ClearOrphans();
}
