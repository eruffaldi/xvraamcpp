#pragma once
#include "aamast.hpp"
#include <iostream>
#include <fstream>

class AAMParser
{
public:
	aamast::AAMMesh * parse(std::string filename);

protected:
	bool binary = false;
	std::string filename;
	void doError(std::string text);

	aamast::Material* parseMaterial();

	bool parseTexture(aamast::Texture* );

	aamast::AAMObject * parseObject(int oid,std::string name);

	bool parseFrame(int frame,aamast::AAMMesh*,aamast::AAMFrame*);

	std::ifstream inf;
};
