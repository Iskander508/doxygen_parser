#ifndef AVG_AE1E5B36_9702_4333_B0CD_F1FCEF3C02D7_STRUCTURE_H__
#define AVG_AE1E5B36_9702_4333_B0CD_F1FCEF3C02D7_STRUCTURE_H__

#include <fstream>
#include <vector>
#include <map>
#include "types.h"

std::vector<_TCHAR> readXMLFromFile(const _TCHAR* filename) {
	std::basic_ifstream<_TCHAR> readFile(filename);
	if (readFile.is_open()) {
		std::basic_string<_TCHAR> fileContent((std::istreambuf_iterator<_TCHAR>(readFile)),
                 std::istreambuf_iterator<_TCHAR>());
		readFile.close();

		std::vector<_TCHAR> result(fileContent.begin(), fileContent.end());
		result.push_back(_T('\0'));
		return result;
	}

	return std::vector<_TCHAR>();
}


class Element {
public:
	Element(const Node& node) : m_node(&node) { Init(); }
	Element(const Node* node = nullptr) : m_node(node) { Init(); }
	Element(const Element& that) : m_node(that.m_node) { m_attributes = that.m_attributes; }
	Element(Element&& that) : m_node(that.m_node) { std::swap(m_attributes, that.m_attributes); }

	const std::map<stringRef, stringRef>& Attributes() const {
		return m_attributes;
	}

	stringRef GetAttribute(const stringRef& name) const {
		if (m_node)	{
			if(Attribute *attr = m_node->first_attribute(name.str())) {
				return attr->value();
			}
		}
		return nullptr;
	}

	stringRef Text(bool recursive = true) const {
		const stringRef firstNodeText = m_node ? stringRef(m_node->value()) : stringRef();
		if (!recursive)	return firstNodeText;

		string result(firstNodeText.str());
		bool firstSubNode = true;
		for (const auto& element: Elements()) {
			if (element.Name() || !firstSubNode) {
				if (firstSubNode) {
					result = element.Text().str();
				} else {
					result.append(element.Text().str());
				}
			}
			firstSubNode = false;
		}

		return result;
	}

	stringRef Name() const {
		return m_node ? stringRef(m_node->name()) : stringRef();
	}

	std::vector<Element> Elements(const stringRef& name = nullptr) const {
		std::vector<Element> result;
		if (m_node)	{
			const _TCHAR* const nameStr = name ? name.str() : nullptr;
			for (Node *node = m_node->first_node(nameStr); node; node = node->next_sibling(nameStr)) {
				result.push_back(node);
			}
		}
		return result;
	}

	Element GetElement(const stringRef& name) const {
		if (m_node)	{
			return m_node->first_node(name.str());
		}
		return nullptr;
	}

	operator bool() const { return m_node != nullptr; }

private:
	void Init() {
		if (m_node)	{
			for (Attribute *attr = m_node->first_attribute(); attr; attr = attr->next_attribute()) {
				m_attributes.insert(std::map<stringRef, stringRef>::value_type(attr->name(), attr->value()));
			}
		}
	}

private:
	std::map<stringRef, stringRef> m_attributes;
	const Node* const m_node;
};

#endif // AVG_AE1E5B36_9702_4333_B0CD_F1FCEF3C02D7_STRUCTURE_H__
