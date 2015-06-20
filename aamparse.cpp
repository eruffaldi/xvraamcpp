#include "aamparse.hpp"

template <typename T,int N>
std::ostream& operator <<(std::ostream & onf, Eigen::Matrix<T,N,1> & o)
{
	for(int i = 0; i < N; i++)
	{
		if(i > 0)
			onf << ' ';
		onf << o(i);
	}
	return onf;
}

template <typename T,int N>
std::istream &operator >>(std::istream & inf, Eigen::Matrix<T,N,1> & o)
{
	for(int i = 0; i < N; i++)
	{
		T q;
		inf >> q;
		o(i) = q;
	}
	return inf;
}

template <typename T>
T lexical_cast(const std::string& str)
{
    T var;
    std::istringstream iss;
    iss.str(str);
    iss >> var;
    // deal with any error bits that may have been set on the stream
    return var;
}

namespace std
{
	template <class T>
static bool getline2(T & inf, std::string & a)
{
	if(!std::getline(inf,a))
		return false;
	if(a.size() > 0 && a[a.size()-1] == '\r')
		 a=a.substr(0,a.size()-1);
	return true;	
}
}

static bool parseProperty(std::istream& inf,std::string & name, std::string &rest)
{
	inf >> name;
	std::getline2(inf,rest);
	int j = 0;
	while(rest[j] == ' ')
		j++;
	if(j > 0)
		rest = rest.substr(j);
	return (bool)inf;
}

aamast::AAMMesh * AAMParser::parse(std::string filename)
{
	inf.open(filename.c_str(),std::ios::binary);
	if(!inf)
		return 0;
	std::string header;
	std::getline2(inf, header);
	if(!(header == "AAM_MESH" || header == "AAM_MESH_MULTIFRAME"))
	{
		std::cerr << "expected AAM_MESH or AAM_MESH_MULTIFRAME, got <" << header << ">" << std::endl;
		return 0;
	}
	std::string materials;
	std::getline2(inf, materials);
	if(materials != "MATERIALS")
	{
		std::cerr << "expected MATERIALS, got" << materials << std::endl;
		return 0;
	}

	std::string matspec;
	int matcount = 0;
	inf >> matspec >> matcount;
	if(!inf || matspec != "MatCount:")
	{
		std::cerr << "expected MatCount:, got" << matspec  << std::endl;
			return 0;
	}
	std::unique_ptr<aamast::AAMMesh> mesh(new aamast::AAMMesh());
	mesh->materials.reserve(matcount);
	for(int i = 0; i < matcount; i++)
	{
		// Mat# index
		std::string mathead;
		int matid;
		inf >> mathead >> matid;
		if(mathead != "Mat#")
		{
			std::cerr << "expected Mat# got " << mathead << std::endl;
			return 0;
		}
		if(matid != i)
		{
			std::cerr << "mismatch material id " << matid << std::endl;
		}
		aamast::Material * m = parseMaterial();
		if(!m)
			return 0;
		mesh->materials.push_back(std::shared_ptr<aamast::Material>(m));
	}
	std::string line;
	std::getline2(inf,line);
	if(line != "ENDMATERIALS")
	{
		std::cerr << "expected ENDMATERIALS got " << line << std::endl;
		return 0;
	}
	std::string name,value;
	parseProperty(inf,name,value);
	if(name != "GEOMETRY")
	{
		std::cerr << "expected GEOMETRY got " << name << std::endl;
		return 0;
	}
	while(true)
	{
		parseProperty(inf,name,value);
		if(name == "NObj:")
		{
			// value is int
			int no = lexical_cast<int>(value);
			mesh->objects.reserve(no);
		}
		else if(name == "NFrames:")
		{
			// value is int
			int no = lexical_cast<int>(value);
			mesh->frames.reserve(no);
		}
		else if(name == "Animation_mode:")
		{
			if(value != "None")
			{
				std::cout << "Animation_mode ignored " << value <<std::endl;
			}
		}
		else if(name == "ENDGEOMETRY")
		{
			break;
		}
		else if(name == "Frame:")
		{
			// value is int
			// then parseFrame
			int no = lexical_cast<int>(value);
			if(no > 0)
			{
				std::cerr << "discarded frames"<<std::endl;
			}
			std::unique_ptr<aamast::AAMFrame> frame(new aamast::AAMFrame());
			if(parseFrame(no,mesh.get(),frame.get()))
			{
				mesh->frames.push_back(std::shared_ptr<aamast::AAMFrame>(frame.release()));
			}
			else
			{
				std::cout << "frame failed " << std::endl;
				return 0;
			}
		}
		else
		{
			std::cerr << "unknown entry " << name << " " << value << std::endl;
		}
	}

	return mesh.release();

}

aamast::Material* AAMParser::parseMaterial()
{
	char c;
	inf >> c;
	if(c != '{')
	{
		std::cerr << "Expected { in material" << std::endl;
		return 0;
	}
	// {
	// Name:
	// Class:
	// 	then make and switch parsing special
	// }
	std::string name,pname,pclass;
	parseProperty(inf,name,pname);
	if(name != "Name:")
	{
		std::cerr << "Expected name in material" << std::endl;
		return 0;
	}
	parseProperty(inf,name,pclass);
	if(name != "Class:")
	{
		std::cerr << "Expected class in material" << std::endl;
		return 0;
	}
	if(pclass == "Multi")
	{
		// NSubs
		int nsubs;
		std::string value;
		parseProperty(inf,name,value);
		if(name == "NSubs:")
			nsubs = lexical_cast<int>(value);
		std::unique_ptr<aamast::MultiMaterial> mat(new aamast::MultiMaterial());
		mat->name = pname;
		mat->materialclass = pclass;
		for (int i = 0; i < nsubs; i++)
		{
			// Expected Sub # 
			// then material
			parseProperty(inf,name,value);
			if(name != "Sub")
			{
				std::cerr << "Waiting Sub" << std::endl;
				break;
			}
			aamast::Material* m = parseMaterial();
			mat->materials.push_back(std::shared_ptr<aamast::Material>(m));
		}
		parseProperty(inf,name,value);
		if(name != "}")
		{
			std::cout << "expecting } found " <<name << std::endl;
			return 0;
		}
		return mat.release();
	}
	else if(pclass == "Standard")
	{
		std::unique_ptr<aamast::StandardMaterial> mat(new aamast::StandardMaterial());
		mat->name = pname;
		mat->materialclass = pclass;
		while(true)
		{
			std::string value;
			parseProperty(inf,name,value);
			if(name == "}")
				break;
			else if(name == "Di:")
			{
				mat->diffuse = lexical_cast<Eigen::Vector3f>(value);
			}
			else if(name == "Am:")
			{
				mat->ambient = lexical_cast<Eigen::Vector3f>(value);
			}
			else if(name == "Sp:")
			{
				mat->specular = lexical_cast<Eigen::Vector3f>(value);
			}
			else if(name == "Tr:")
				mat->transparency = lexical_cast<float>(value);
			else if(name == "Sh:")
				mat->shininess = lexical_cast<float>(value);
			else if(name == "Tx:" || name == "TS:")
			{
				if(value == "Y")
				{
					std::cout <<"\tparse textur\n";
					parseTexture((name == "Tx:" ? &mat->tx : &mat->ts));
					std::cout << "\tattached " << mat->tx.filename << " " << mat->ts.filename << std::endl;
				}
			}
			else
				std::cout << "unsupported " << name <<  " " << value << std::endl;

		}
		return mat.release();

	}
	else
	{
		std::cerr << "Unknown class in material <" << pclass <<">"<< std::endl;
		return 0;

	}
	return 0;
}


aamast::AAMObject * AAMParser::parseObject(int oid,std::string xname)
{
	std::string name,value;
	int parid;
	parseProperty(inf,name,value);
	if(name != "Par:")
	{
		std::cout << "Expected Par got " << name << " " << value << std::endl;
		return 0;
	}
	parid = lexical_cast<int>(value);
	parseProperty(inf,name,value);
	if(name != "{")
	{
		std::cout << "Expected { got " << name << " " << value << std::endl;
		return 0;
	}
	std::unique_ptr<aamast::AAMObject> obj(new aamast::AAMObject());
	obj->identifier = oid;
	obj->name = xname;
	obj->parentidentifier = parid;
	std::cout << "parser::createdobject <" << xname << "> " << oid << " " << parid << std::endl;
	while(true)
	{
		parseProperty(inf,name,value);
		if(name == "MatID:")		
		{
			obj->materialidentifier = lexical_cast<int>(value);
			std::cout << "\tmaterial " << obj->materialidentifier << std::endl;
		}
		else if(name == "UserProp:")
		{

		}
		else if(name == "V_List:")
		{
			std::string pa;
			int vtxcount = lexical_cast<int>(value);
			obj->vertices.resize(vtxcount);
			for(int i = 0; i < vtxcount; i++)
			{
				getline2(inf,pa);
				obj->vertices[i] = lexical_cast<Eigen::Vector3f>(pa);
			}
			std::cout << "\tV_List done " << " " << obj->vertices.size() << " vertices" << std::endl;
		}
		else if(name == "TV_List:")
		{
			std::string pa;
			int tvcount = lexical_cast<int>(value);
			obj->texturecoordinates.resize(tvcount);
			for(int i = 0; i < tvcount; i++)
			{
				getline2(inf,pa);
				obj->texturecoordinates[i] = lexical_cast<Eigen::Vector2f>(pa);
			}
			std::cout << "\tTV_List done" << " " << obj->texturecoordinates.size() << " tex coordinates" << std::endl;
		}		
		else if(name == "I_List:")
		{
			std::string pa;
			Eigen::Vector2i info = lexical_cast<decltype(info)>(value);
			std::cout << "\tI_List " << info  << std::endl;
			obj->groups.reserve(info(1));
			obj->ivertices.resize(info(0));
			obj->smoothinggroup.resize(info(0));
			// not for the textures
			int iface = -1;

			for(int i = 0; i < info(1); i++)
			{
				parseProperty(inf,name,value);
				if(name != "NEWGROUP:")
				{
					std::cerr << "\tExpected NEWGROUP: but " << name << " " << value << std::endl;
					return 0;
				}
				std::unique_ptr<aamast::AAMGroup> group(new aamast::AAMGroup());
				group->materialsubidentifier = lexical_cast<int>(value);
				group->startface = iface+1;

				// Optimization: number of faces in a group cannot be established directly, but we can
				// store all the faces of a mesh in a single structure and then associate them to the different groups
				int ti = 0;		
				int maxti = 0;		
				while(true)
				{
					parseProperty(inf,name,value);
					if(name == "ENDGROUP")
					{
						group->count = iface+1-group->startface;
						group->textureunits = ti;
						std::cout << "\tENDGROUP " << " matid:" << group->materialsubidentifier << " from:" << group->startface << " to: " << group->startface+group->count-1 <<  " count:" << group->count << " texunits:" << group->textureunits << std::endl;
						obj->groups.push_back(std::shared_ptr<aamast::AAMGroup>(group.release()));
						break;
					}
					else if(name == "I:")
					{
						++iface;
						ti = 0;
						Eigen::Vector4i q = lexical_cast<Eigen::Vector4i>(value);
						obj->ivertices[iface] = Eigen::Vector3i(q(0),q(1),q(2));
						obj->smoothinggroup[iface] = q(3);
						ti = 0;
					}
					else if(name == "TI:")
					{
						Eigen::Vector3i q = lexical_cast<Eigen::Vector3i>(value);
						if(ti >= obj->itexturecoordinates.size())
							obj->itexturecoordinates.push_back(std::vector<Eigen::Vector3i>(info(0)));
						obj->itexturecoordinates[ti++][iface] = q;
					}
				}
			}
			std::cout << "\tdone I_List" << std::endl;
		}
		else if(name == "}")
			break;
	}
	
	return obj.release();
	
}


bool AAMParser::parseTexture(aamast::Texture* te)
{
	// FN,Fi,In,...
	// }
	char c;
	inf >> c;
	if(c != '{')
	{
		std::cerr << "Expected { in material" << std::endl;
		return false;
	}
	while(true)
	{
		std::string name,value;
		parseProperty(inf,name,value);
		if(name == "}")
			break;
		else if(name == "FN:")
		{
			if(value.at(0) == '/')
				value = value.substr(1);
			te->filename = value;
			std::cout << "texture processing " << value << std::endl;
		}
		else if(name == "Fi:")
		{
			if(value != "PYRAMIDAL")
			{
			std::cout << "unsupported " << name <<  " " << value << std::endl;
			}
			te->mapping = value;
		}
		else if(name == "In:")
			te->intensity = lexical_cast<float>(value);
		else if(name == "UA:")
			te->uvwA.x() = lexical_cast<float>(value);
		else if(name == "VA:")
			te->uvwA.y() = lexical_cast<float>(value);
		else if(name == "WA:")
			te->uvwA.z() = lexical_cast<float>(value);
		else if(name == "UO:")
			te->uvO.x() = lexical_cast<float>(value);
		else if(name == "VO:")
			te->uvO.y() = lexical_cast<float>(value);
		else if(name == "UT:")
			te->uvT.x() = lexical_cast<float>(value);
		else if(name == "VT:")
			te->uvT.y() = lexical_cast<float>(value);
		else
			std::cout << "unsupported " << name <<  " " << value << std::endl;
	}
	return true;


}
bool AAMParser::parseFrame(int no, aamast::AAMMesh * mesh, aamast::AAMFrame * frame)
{
	// loop over many definitions as follows:
	std::string name,value;
	parseProperty(inf, name,value);
	if(name != "{")
	{
			std::cerr << "parseFrame unexpected " << name << " " << value << std::endl;
			return false;
	}

	while(true)
	{
		parseProperty(inf, name,value);
		if(name == "Obj:")
		{
			int oid;
			std::string oname;
			std::istringstream ins(value);
			ins >> oid >> oname;
			aamast::AAMObject * o = parseObject(oid,oname);
			if(no > 0)
			{
				delete o;
				continue;
			}
			if(!o)
			{
				std::cerr << "AAMObject in frame failed"<< std::endl;
				return false;
			}
			if(no == 0)
				mesh->objects.push_back(std::shared_ptr<aamast::AAMObject>(o));
			else
				delete o;
		}
		else if(name == "}")
		{
			return true;
		}
		else
		{
			std::cerr << "parseFrame unexpected " << name << " " << value << std::endl;
			return false;
		}

	}
	return true;
}


