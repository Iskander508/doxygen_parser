// doxygenParser.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include "xml/structure.h"
#include <iostream>
#include "FileSystem.h"
#include "ClassManager.h"

void printElement(const Element& element, const int indent = 0) {
	const string indentation(indent*2, _T(' '));
	std::wcout << indentation << _T("node: ") << element.Name().str() << std::endl;

	if (element.Text()) {
		std::wcout << indentation << _T("text: ") << element.Text().str() << std::endl;
	}

	auto& attributes = element.Attributes();
	if (!attributes.empty()) {
		std::wcout << indentation << _T("attributes: ") << std::endl;
		for (const auto& attribute: attributes) {
			std::wcout << indentation << _T(" ") << attribute.first.str() << _T(": ") << attribute.second.str() << std::endl;
		}
	}

	auto subitems = element.Elements();
	if (!subitems.empty()) {
		std::wcout << indentation << _T("subitems: ") << std::endl;
		for (const auto& e: subitems) {
			printElement(e, indent + 1);
		}
	}
}


int _tmain(int argc, _TCHAR* argv[])
{

	if (argc == 2) {

		ClassManager classManager(argv[1]);

		std::wcout << _T("Fetching classes...") << std::endl;
		for (const auto& file : FileSystem::GetFiles(string(argv[1]), _T("xml"))) {
			auto fileContent = readXMLFromFile(file.c_str());

			using namespace rapidxml;
			xml_document<_TCHAR> doc;
			doc.parse<0>(fileContent.data());

			Element doxygenNode = Element(doc.first_node(_T("doxygen")));
			for (const auto& def : doxygenNode.Elements(_T("compounddef"))) {
				if (def.GetAttribute(_T("language")) == _T("C++")) {
					classManager.ProcessDef(def);
				}
			}
		}

		std::wcout << _T("Running classes analysis...") << std::endl;
		classManager.Initialize();

		std::wcout << _T("Running source files analysis...") << std::endl;
		const auto files = FileSystem::GetFiles(string(argv[1]), _T("xml"));

		#pragma omp parallel for
		for (int i = 0; i < files.size(); i++) {
			const auto& file = files[i];
			auto fileContent = readXMLFromFile(file.c_str());

			using namespace rapidxml;
			xml_document<_TCHAR> doc;
			doc.parse<0>(fileContent.data());

			Element doxygenNode = Element(doc.first_node(_T("doxygen")));
			for (const auto& def : doxygenNode.Elements(_T("compounddef"))) {
				if (def.GetAttribute(_T("language")) == _T("C++") && def.GetAttribute(_T("kind")) == _T("file")) {
					classManager.ProcessFileDef(def);
				}
			}
		}

		std::wcout << _T("Writing json output...") << std::endl;
		classManager.WriteClassesJson();
		classManager.WriteSingleClassJsons();

		std::wcout << _T("Done.") << std::endl;

	}
	return 0;
}

