// doxygenParser.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include "xml/structure.h"
#include <iostream>
#include "FileSystem.h"
#include "JsonWriter.h"

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

		std::vector<Class> classes;
		std::vector<string> namespaces;

		for (const auto& file : FileSystem::GetFiles(string(argv[1]), _T("xml"))) {
			auto fileContent = readXMLFromFile(file.c_str());

			using namespace rapidxml;
			xml_document<_TCHAR> doc;
			doc.parse<0>(fileContent.data());

			Element doxygenNode = Element(doc.first_node(_T("doxygen")));
			for (const auto& def : doxygenNode.Elements(_T("compounddef"))) {
				if (def.GetAttribute(_T("language")) == _T("C++")) {
					const stringRef kind = def.GetAttribute(_T("kind"));

					if (kind == _T("namespace")) {
						namespaces.push_back(string(def.GetElement(_T("compoundname")).Text().str()));
					} else if (kind == _T("class") || kind == _T("struct")) {
						Class newClass;
						newClass.doxygenId = def.GetAttribute(_T("id")).str();
						if (def.GetAttribute(_T("abstract")) == _T("yes")) {
							newClass.type = Class::INTERFACE;
						} else {
							newClass.type = kind == _T("class") ? Class::CLASS : Class::STRUCT;
						}

						newClass.name = def.GetElement(_T("compoundname")).Text().str();

						for (const auto& parent : def.Elements(_T("basecompoundref"))) {
							EProtectionLevel protectionLevel = PACKAGE;
							stringRef prot = parent.GetAttribute(_T("prot"));

							if (prot == _T("public")) {
								protectionLevel = PUBLIC;
							} else if (prot == _T("protected")) {
								protectionLevel = PROTECTED;
							} else if (prot == _T("private")) {
								protectionLevel = PRIVATE;
							}

							newClass.inheritance.insert(std::map<string, EProtectionLevel>::value_type(string(parent.Text().str()), protectionLevel));
						}


						for (const auto& section : def.Elements(_T("sectiondef"))) {
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
									method.protectionLevel = protectionLevel;
									method.returnType = member.GetElement(_T("type")).Text().str();
									method.Const = member.GetAttribute(_T("const")) == _T("yes");
									method.Virtual = member.GetAttribute(_T("virt")) != _T("non-virtual");
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
									newClass.methods.push_back(std::move(method));
								} else if (kind == _T("variable")) {
									Member m;
									m.protectionLevel = protectionLevel;
									m.name = member.GetElement(_T("name")).Text().str();
									m.type = member.GetElement(_T("type")).Text().str();
									newClass.members.push_back(std::move(m));}
							}
						}

						classes.push_back(std::move(newClass));
					}
				}
			}
		}

		JsonWriter writer(argv[1]);
		writer.Initialize(namespaces, classes);

		for (const auto& file : FileSystem::GetFiles(string(argv[1]), _T("xml"))) {
			auto fileContent = readXMLFromFile(file.c_str());

			using namespace rapidxml;
			xml_document<_TCHAR> doc;
			doc.parse<0>(fileContent.data());

			Element doxygenNode = Element(doc.first_node(_T("doxygen")));
			for (const auto& def : doxygenNode.Elements(_T("compounddef"))) {
				if (def.GetAttribute(_T("language")) == _T("C++") && def.GetAttribute(_T("kind")) == _T("file")) {
					writer.ProcessFileDef(def);
				}
			}
		}

		writer.WriteClassesJson();
		writer.WriteSingleClassJsons();

	}
	return 0;
}

