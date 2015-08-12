#ifndef JSON_WRITER_H___
#define JSON_WRITER_H___

#include "types.h"
#include <vector>
#include <map>
#include <fstream>

struct JsonWriter {
	JsonWriter(const stringRef& filePath, const stringRef& classId = nullptr) : m_file(filePath.str()), m_classId(classId.str()) {}
	~JsonWriter();

	void WriteNode(const stringRef& id, const stringRef& shortName, const stringRef& longName, const stringRef& hoverName, const stringRef& type,
		const stringRef& parent = nullptr, const stringRef& reference = nullptr, const stringRef& filename = nullptr,
		const std::vector<string>& classes = std::vector<string>());
	void WriteEdge(const stringRef& sourceId, const stringRef& targetId, const stringRef& type,
		const stringRef& description = nullptr, const std::vector<string>& classes = std::vector<string>());

	void ClearOrphans();

private:
	struct SNode {
		string shortName;
		string longName;
		string hoverName;
		string type;
		string parent;
		string reference;
		string filename;
		std::vector<string> classes;
	};

	struct SEdge {
		string sourceId;
		string targetId;
		string type;
		string description;
		std::vector<string> classes;
	};

private:
	std::map<string, SNode> m_nodes;
	std::vector<SEdge> m_edges;
	std::basic_ofstream<_TCHAR> m_file;
	string m_classId;
};

#endif // JSON_WRITER_H___
