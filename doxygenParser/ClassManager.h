#ifndef CLASS_MANAGER_H__
#define CLASS_MANAGER_H__

#include "types.h"
#include "xml/structure.h"
#include <vector>
#include <map>
#include <set>
#include <mutex>


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
	bool Override;

	struct Param {
		string type;
		string name;
	};
	std::vector<Param> params;
	EProtectionLevel protectionLevel;

	string locationFile;
	string bodyBeginLine;
	string bodyEndLine;

	bool operator==(const Method& that) const { return doxygenId == that.doxygenId; }
	bool operator!=(const Method& that) const { return !operator==(that); }
};

struct Inheritance {
	string classId;
	EProtectionLevel protLevel;
	bool Virtual;
};

struct Class {
	enum EType {
		INTERFACE,
		STRUCT,
		CLASS
	};

	string name;
	string doxygenId;
	string filename; //!< declaration file
	EType type;
	bool templated;
	std::vector<Inheritance> inheritance;
	std::vector<Method> methods;
	std::vector<Member> members;

	Class() : templated(false), type(CLASS) {}
};

struct ClassManager {
	ClassManager(const stringRef& outputDir) : m_outputDir(outputDir.str()) {}

	void Initialize();

	void ProcessDef(const Element& classDef);
	void ProcessFileDef(const Element& fileDef);

	void WriteClassesJson();
	void WriteSingleClassJsons() const;

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
		string connectedMember;
		EClassConnectionType type;
		bool Virtual;
		EProtectionLevel protectionLevel;
	};

	enum EMemberUsageType {
		METHOD_CALL,
		MEMBER_ACCESS,
		CLASS_USAGE
	};

	struct MemberUsage {
		string sourceMethodId; //!< doxygenId of calling method
		string connectionCode; //!< source line of usage/parameter/return value
		string targetId; //!< either doxygen method id or member name
		EMemberUsageType type;
		bool certain; //!< whether the access is evident from the 

		MemberUsage() : certain(true) {}
	};

	struct ClassEntry {
		string name;
		Class data;
		string namespaceId;
		string parentId;
		std::vector<ClassConnection> connections; // connection to other classes (via inheritance or composition via members)
		std::vector<MemberUsage> memberUsages;
		std::map<string, string> methodOverrides; //!< method doxygenId -> interface id
		bool utility; //!< flag whether this class is utility only

		ClassEntry() : utility(false) {}
	};

private:
	void CalculateNamespaces(const std::vector<string>& namespaces);
	void CalculateClasses(const std::vector<Class>& classes);
	void CalculateMethods();
	void ClearOrphanItems();

	static string GetLastId(const string& name);
	static string GetWithoutLastId(const string& name);

	std::vector<ClassConnection> GetConnections(const string& type, const string& namespaceId, const std::set<string>& ids, EProtectionLevel protLevel, bool Virtual = false) const;
	void WriteSingleClassJson(const stringRef& id) const;

	// search string -> class id
	std::map<string, string> GetUsableClasses(const stringRef& classId, const stringRef& namespaceId) const;

private:
	std::map<string, Namespace> m_namespaces; //!< id -> Namespace
	std::map<string, ClassEntry> m_classes; //!< id -> ClassEntry
	string m_outputDir;
	std::mutex m_lock;

	std::vector<Class> initClasses;
	std::vector<string> initNamespaces;
};

#endif // CLASS_MANAGER_H__
