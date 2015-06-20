#include "aamparse.hpp"
#include <iostream>

int main(int argc, char const *argv[])
{
	AAMParser parser;
	for(int i = 1; i < argc; i++)
	{
		std::cout << "Parsing " << argv[i] << std::endl;
		aamast::AAMMesh * mesh = parser.parse(argv[i]);
		if(mesh)
		{
			std::cout << "materials " << mesh->materials.capacity() << std::endl;
		}
		delete mesh;
	}
	return 0;
}