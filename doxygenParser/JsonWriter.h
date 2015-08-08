#ifndef JSON_WRITER_H__
#define JSON_WRITER_H__

#include "types.h"
#include <vector>
#include <map>
#include <set>


enum EProtectionLevel {
	PRIVATE,
	PROTECTED,
	PACKAGE,
	PUBLIC
};

struct Member {
	string name;
	string type;
	EProtectionLevel protectionLevel;
};

struct Method {
	string name;
	string doxygenId;
	string returnType;
	bool Const;
	bool Virtual;

	struct Param {
		string type;
		string name;
	};
	std::vector<Param> params;
	EProtectionLevel protectionLevel;
};

struct Class {
	enum EType {
		INTERFACE,
		STRUCT,
		CLASS
	};

	string name;
	string doxygenId;
	EType type;
	std::map<string, EProtectionLevel> inheritance;
	std::vector<Method> methods;
	std::vector<Member> members;
};

struct JsonWriter {
	JsonWriter(const stringRef& outputDir) : m_outputDir(outputDir.str()) {}

	void Initialize(const std::vector<string>& namespaces, const std::vector<Class>& classes);

	void Write();

private:
	struct Namespace {
		string name;
		string parentId;
	};

	enum EClassConnectionType {
		DIRECT_INHERITANCE,
		INDIRECT_INHERITANCE,
		MEMBER_ITEM
	};

	struct ClassConnection {
		string targetId;
		string connectionCode;
		EClassConnectionType type;
	};

	struct ClassEntry {
		string name;
		string doxygenId;
		Class::EType type;
		string namespaceId;
		string parentId;
		std::vector<ClassConnection> connections;
	};

private:
	void CalculateNamespaces(const std::vector<string>& namespaces);
	void CalculateClasses(const std::vector<Class>& classes);

	static string GetLastId(const string& name);
	static string GetWithoutLastId(const string& name);

	std::vector<ClassConnection> GetConnections(const string& type, const string& namespaceId, const std::set<string>& ids, EProtectionLevel protLevel) const;
	string WriteNode(const stringRef& id, const stringRef& name, const stringRef& parent, const stringRef& type) const;
	string WriteEdge(const stringRef& source, const stringRef& target,  const stringRef& code, const stringRef& type) const;

private:
	std::map<string, Namespace> m_namespaces; //!< id -> Namespace
	std::map<string, ClassEntry> m_classes; //!< id -> ClassEntry
	string m_outputDir;
};

#endif // JSON_WRITER_H__
