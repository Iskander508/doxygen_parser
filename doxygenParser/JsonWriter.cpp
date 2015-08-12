#include "JsonWriter.h"
#include <sstream>
#include <set>
#include "xml/structure.h"

string escape(string s)
{
	string result = replaceAll(std::move(s), _T("\\"), _T("\\\\"));
	result = replaceAll(std::move(result), _T("\""), _T("\\\""));
	result = replaceAll(std::move(result), _T("\n"), _T("\\n"));
	result = replaceAll(std::move(result), _T("\t"), _T("\\t"));
	return result;
}


JsonWriter::~JsonWriter()
{
	m_file << _T("{\"nodes\": [") << std::endl;
	{
		bool first = true;
		for (const auto& node: m_nodes) {
			if (!first) {
				m_file << _T(",") << std::endl;
			}
			first = false;

			std::basic_ostringstream<_TCHAR> s;
			s	<< _T("{\"id\":\"") << escape(node.first)
				<< _T("\", \"shortName\":\"") << escape(node.second.shortName)
				<< _T("\", \"type\":\"") << escape(node.second.type);
			if (!node.second.parent.empty()) {
				s << _T("\", \"parent\":\"") << escape(node.second.parent);
			}
			if (!node.second.longName.empty()) {
				s << _T("\", \"longName\":\"") << escape(node.second.longName);
			}
			if (!node.second.hoverName.empty()) {
				s << _T("\", \"hoverName\":\"") << escape(node.second.hoverName);
			}
			if (!node.second.reference.empty()) {
				s << _T("\", \"reference\":\"") << escape(node.second.reference);
			}
			if (!node.second.filename.empty()) {
				s << _T("\", \"filename\":\"") << escape(node.second.filename);
			}
			if (!node.second.classes.empty()) {
				s << _T("\", \"classes\":[");
				bool first = true;
				for (const auto& c : node.second.classes) {
					if (!first) {
						s << _T(",");
					}
					first = false;
					s << _T("\"") << escape(c) << _T("\"");
				}
				s << _T("]}");
			} else s << _T("\"}");

			m_file << s.str();
		}
	}

	m_file << std::endl << _T("], \"edges\": [") << std::endl;
	{
		bool first = true;
		for (const auto& edge: m_edges) {
			if (m_nodes.find(edge.sourceId) == m_nodes.end() || m_nodes.find(edge.targetId) == m_nodes.end()) continue;

			if (!first) {
				m_file << _T(",") << std::endl;
			}
			first = false;

			std::basic_ostringstream<_TCHAR> s;
			s	<< _T("{\"source\":\"") << escape(edge.sourceId)
				<< _T("\", \"target\":\"") << escape(edge.targetId)
				<< _T("\", \"type\":\"") << escape(edge.type);
			if (!edge.description.empty()) {
				s << _T("\", \"description\":\"") << escape(edge.description);
			}
			if (!edge.classes.empty()) {
				s << _T("\", \"classes\":[");
				bool first = true;
				for (const auto& c : edge.classes) {
					if (!first) {
						s << _T(",");
					}
					first = false;
					s << _T("\"") << escape(c) << _T("\"");
				}
				s << _T("]}");
			} else s << _T("\"}");

			m_file << s.str();
		}
	}

	if (m_classId.empty()) {
		m_file << std::endl << _T("]}") << std::endl;
	} else {
		m_file << std::endl << _T("], \"class\":\"") << escape(m_classId) << _T("\"}") << std::endl;
	}
	
	m_file.close();
}

void JsonWriter::WriteNode(const stringRef& id, const stringRef& shortName, const stringRef& longName, const stringRef& hoverName, const stringRef& type,
		const stringRef& parent, const stringRef& reference, const stringRef& filename, const std::vector<string>& classes)
{
	SNode node;
	node.shortName = shortName.str();
	node.longName = longName.str();
	node.hoverName = hoverName.str();
	node.type = type.str();
	node.parent = parent.str();
	node.reference = reference.str();
	node.filename = filename.str();
	node.classes = classes;

	m_nodes.insert(std::map<string, SNode>::value_type(id.str(), std::move(node)));
}

void JsonWriter::WriteEdge(const stringRef& sourceId, const stringRef& targetId, const stringRef& type,
		const stringRef& description, const std::vector<string>& classes)
{
	SEdge edge;
	edge.sourceId = sourceId.str();
	edge.targetId = targetId.str();
	edge.type = type.str();
	edge.description = description.str();
	edge.classes = classes;

	m_edges.push_back(std::move(edge));
}

void JsonWriter::ClearOrphans()
{
	while(true) {
		std::set<string> ids;
		for (const auto& node: m_nodes) {
			ids.insert(node.first);
		}

		for (const auto& edge: m_edges) {
			ids.erase(edge.sourceId);
			ids.erase(edge.targetId);
		}

		for (const auto& node: m_nodes) {
			if (ids.find(node.first) == ids.end()) {
				string parent = node.second.parent;
				do {
					ids.erase(parent);
					parent = m_nodes[parent].parent;
				} while (!parent.empty());
			}
		}

		if (ids.empty()) break;

		for (const auto& id: ids) {
			m_nodes.erase(id);
		}
	}
}
